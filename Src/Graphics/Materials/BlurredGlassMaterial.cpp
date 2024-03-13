#include "BlurredGlassMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../Vertex.hpp"
#include "MeshDrawArgs.hpp"
#include "StaticPropMaterial.hpp"

static eg::Pipeline pipelineBlurry;
static eg::Pipeline pipelineNotBlurry;
static eg::Pipeline pipelineDepthOnly;
static eg::Pipeline pipelineEditor;

static const eg::DescriptorSetBinding descriptorSetBindings[] = {
	eg::DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingType::UniformBuffer,
		.shaderAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingType::Texture,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 2,
		.type = eg::BindingType::UniformBuffer,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
};

static void OnInit()
{
	const eg::ShaderModuleAsset& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/BlurredGlass.fs.glsl");

	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.topology = eg::Topology::TriangleList;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.enableDepthTest = true;

	pipelineCI.enableDepthWrite = true;
	pipelineCI.label = "BlurredGlass[DepthOnly]";
	pipelineCI.numColorAttachments = 0;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineDepthOnly = eg::Pipeline::Create(pipelineCI);

	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.label = "BlurredGlass[Blurry]";
	pipelineCI.fragmentShader = fs.ToStageInfo("VBlur");
	pipelineBlurry = eg::Pipeline::Create(pipelineCI);

	pipelineCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::Zero,
		/* srcAlpha */ eg::BlendFactor::Zero,
		/* dstColor */ eg::BlendFactor::SrcColor,
		/* dstAlpha */ eg::BlendFactor::One
	);
	pipelineCI.label = "BlurredGlass[NotBlurry]";
	pipelineCI.enableDepthWrite = false;
	pipelineCI.fragmentShader = fs.ToStageInfo("VNoBlur");
	pipelineNotBlurry = eg::Pipeline::Create(pipelineCI);

	pipelineCI.label = "BlurredGlass[Editor]";
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineEditor = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	pipelineBlurry.Destroy();
	pipelineNotBlurry.Destroy();
	pipelineDepthOnly.Destroy();
	pipelineEditor.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

BlurredGlassMaterial::BlurredGlassMaterial(eg::ColorLin color, bool isBlurry) : m_isBlurry(isBlurry)
{
	const float parametersData[] = { color.r, color.g, color.b, color.a };
	m_parametersBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(parametersData), parametersData);

	m_descriptorSet = eg::DescriptorSet(descriptorSetBindings);
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GlassHex.png"), 1, &commonTextureSampler);
	m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 2);
}

size_t BlurredGlassMaterial::PipelineHash() const
{
	return typeid(BlurredGlassMaterial).hash_code() + m_isBlurry;
}

std::optional<bool> BlurredGlassMaterial::ShouldRenderBlurry(const MeshDrawArgs& meshDrawArgs) const
{
	if (meshDrawArgs.drawMode == MeshDrawMode::Editor)
		return false;
	if (meshDrawArgs.drawMode == MeshDrawMode::TransparentBeforeBlur)
		return false;
	if (meshDrawArgs.drawMode == MeshDrawMode::TransparentFinal)
		return m_isBlurry && meshDrawArgs.glassBlurTexture.handle != nullptr;
	return {};
}

bool BlurredGlassMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	const MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);

	if (mDrawArgs->drawMode == MeshDrawMode::BlurredGlassDepthOnly && m_isBlurry)
	{
		cmdCtx.BindPipeline(pipelineDepthOnly);
		RenderSettings::instance->BindVertexShaderDescriptorSet();
		return true;
	}

	bool blurry;
	if (mDrawArgs->drawMode == MeshDrawMode::Editor)
	{
		blurry = false;
		cmdCtx.BindPipeline(pipelineEditor);
	}
	else
	{
		if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
			blurry = false;
		else if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
			blurry = m_isBlurry && mDrawArgs->glassBlurTexture.handle != nullptr;
		else
			return false;

		cmdCtx.BindPipeline(blurry ? pipelineBlurry : pipelineNotBlurry);
	}

	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);

	if (blurry)
	{
		cmdCtx.BindTexture(mDrawArgs->glassBlurTexture, 1, 0, &framebufferNearestSampler);
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindTexture(
			mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 1, 0, &framebufferNearestSampler
		);
	}
	else
	{
		cmdCtx.BindTexture(blackPixelTexture, 1, 0, &framebufferNearestSampler);
	}

	return true;
}

bool BlurredGlassMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}

eg::IMaterial::VertexInputConfiguration BlurredGlassMaterial::GetVertexInputConfiguration(const void* drawArgs) const
{
	return VertexInputConfig_SoaPXNTI;
}
