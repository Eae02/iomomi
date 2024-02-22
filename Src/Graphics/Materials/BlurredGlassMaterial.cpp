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
	pipelineCI.enableDepthWrite = false;
	pipelineCI.label = "BlurredGlass[Blurry]";
	pipelineCI.fragmentShader = fs.GetVariant("VBlur");
	pipelineBlurry = eg::Pipeline::Create(pipelineCI);

	pipelineCI.blendStates[0] = eg::BlendState(
		eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::Zero,
		/* srcAlpha */ eg::BlendFactor::Zero,
		/* dstColor */ eg::BlendFactor::SrcColor,
		/* dstAlpha */ eg::BlendFactor::One);
	pipelineCI.label = "BlurredGlass[NotBlurry]";
	pipelineCI.enableDepthWrite = false;
	pipelineCI.fragmentShader = fs.GetVariant("VNoBlur");
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

size_t BlurredGlassMaterial::PipelineHash() const
{
	return typeid(BlurredGlassMaterial).hash_code() + isBlurry;
}

std::optional<bool> BlurredGlassMaterial::ShouldRenderBlurry(const MeshDrawArgs& meshDrawArgs) const
{
	if (meshDrawArgs.drawMode == MeshDrawMode::Editor)
		return false;
	if (meshDrawArgs.drawMode == MeshDrawMode::TransparentBeforeBlur)
		return false;
	if (meshDrawArgs.drawMode == MeshDrawMode::TransparentFinal)
		return isBlurry && meshDrawArgs.glassBlurTexture.handle != nullptr;
	return {};
}

float* clearGlassHexAlpha = eg::TweakVarFloat("glass_hex_alpha", 0.8f, 0.0f, 1.0f);

bool BlurredGlassMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	const MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);

	if (mDrawArgs->drawMode == MeshDrawMode::BlurredGlassDepthOnly && isBlurry)
	{
		cmdCtx.BindPipeline(pipelineDepthOnly);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
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
			blurry = isBlurry && mDrawArgs->glassBlurTexture.handle != nullptr;
		else
			return false;

		cmdCtx.BindPipeline(blurry ? pipelineBlurry : pipelineNotBlurry);
	}

	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);

	cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/GlassHex.png"), 0, 1, &commonTextureSampler);

	const float pcData[6] = {
		color.r,
		color.g,
		color.b,
		*clearGlassHexAlpha,
		1.0f / (mDrawArgs->rtManager ? mDrawArgs->rtManager->ResX() : 1),
		1.0f / (mDrawArgs->rtManager ? mDrawArgs->rtManager->ResY() : 1),
	};

	cmdCtx.PushConstants(0, (blurry ? 6 : 4) * sizeof(float), pcData);

	if (blurry)
	{
		cmdCtx.BindTexture(mDrawArgs->glassBlurTexture, 0, 2, &framebufferNearestSampler);
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindTexture(
			mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 0, 2, &framebufferNearestSampler);
	}
	else
	{
		cmdCtx.BindTexture(blackPixelTexture, 0, 2, &framebufferNearestSampler);
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
