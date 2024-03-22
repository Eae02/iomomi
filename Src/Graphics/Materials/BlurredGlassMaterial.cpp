#include "BlurredGlassMaterial.hpp"

#include "../../Editor/EditorGraphics.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"
#include "../RenderTargets.hpp"
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
		.type = eg::BindingTypeUniformBuffer{},
		.shaderAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 1,
		.type = eg::BindingTypeTexture{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 2,
		.type = eg::BindingTypeUniformBuffer{},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 3,
		.type = eg::BindingTypeSampler::Default,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 4,
		.type = eg::BindingTypeSampler::Default,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	eg::DescriptorSetBinding{
		.binding = 5,
		.type = eg::BindingTypeSampler::Nearest,
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
	pipelineCI.enableDepthTest = true;

	pipelineCI.enableDepthWrite = true;
	pipelineCI.label = "BlurredGlass[DepthOnly]";
	pipelineCI.numColorAttachments = 0;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineDepthOnly = eg::Pipeline::Create(pipelineCI);

	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.descriptorSetBindings[0] = descriptorSetBindings;
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
	pipelineCI.depthStencilUsage = eg::TextureUsage::FramebufferAttachment;
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
	m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/GlassHex.png"), 1);
	m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 2);
	m_descriptorSet.BindSampler(samplers::linearRepeatAnisotropic, 3);
	m_descriptorSet.BindSampler(samplers::linearClamp, 4);
	m_descriptorSet.BindSampler(samplers::nearestClamp, 5);
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
		return m_isBlurry && meshDrawArgs.renderTargets->blurredGlassDepthStage.has_value();
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
			blurry = m_isBlurry && mDrawArgs->renderTargets->blurredGlassDepthStage.has_value();
		else
			return false;

		cmdCtx.BindPipeline(blurry ? pipelineBlurry : pipelineNotBlurry);
	}

	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);

	if (blurry)
	{
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->glassBlurTextureDescriptorSet.value(), 1);
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindDescriptorSet(mDrawArgs->renderTargets->blurredGlassDepthTextureDescriptorSet.value(), 1);
	}
	else
	{
		cmdCtx.BindDescriptorSet(blackPixelTextureDescriptorSet, 1);
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
