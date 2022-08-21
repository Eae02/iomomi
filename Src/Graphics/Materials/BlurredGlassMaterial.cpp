#include "BlurredGlassMaterial.hpp"
#include "StaticPropMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"
#include "../GraphicsCommon.hpp"

static eg::Pipeline pipelineBlurry;
static eg::Pipeline pipelineNotBlurry;
static eg::Pipeline pipelineDepthOnly;

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
	pipelineDepthOnly = eg::Pipeline::Create(pipelineCI);
	pipelineDepthOnly.FramebufferFormatHint(eg::Format::Undefined, GB_DEPTH_FORMAT);
	
	pipelineCI.enableDepthWrite = false;
	pipelineCI.label = "BlurredGlass[Blurry]";
	pipelineCI.fragmentShader = fs.GetVariant("VBlur");
	pipelineBlurry = eg::Pipeline::Create(pipelineCI);
	pipelineBlurry.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	pipelineBlurry.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
	
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::Zero,
		/* srcAlpha */ eg::BlendFactor::Zero,
		/* dstColor */ eg::BlendFactor::SrcColor,
		/* dstAlpha */ eg::BlendFactor::One);
	pipelineCI.label = "BlurredGlass[NotBlurry]";
	pipelineCI.enableDepthWrite = false;
	pipelineCI.fragmentShader = fs.GetVariant("VNoBlur");
	pipelineNotBlurry = eg::Pipeline::Create(pipelineCI);
	pipelineNotBlurry.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	pipelineNotBlurry.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
}

static void OnShutdown()
{
	pipelineBlurry.Destroy();
	pipelineNotBlurry.Destroy();
	pipelineDepthOnly.Destroy();
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
	return { };
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
	
	std::optional<bool> blurry = ShouldRenderBlurry(*mDrawArgs);
	if (!blurry.has_value())
		return false;
	
	cmdCtx.BindPipeline(*blurry ? pipelineBlurry : pipelineNotBlurry);
	
	cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/GlassHex.png"), 0, 1);
	
	const float pcData[6] = 
	{
		color.r,
		color.g,
		color.b,
		*clearGlassHexAlpha,
		1.0f / (mDrawArgs->rtManager ? mDrawArgs->rtManager->ResX() : 1),
		1.0f / (mDrawArgs->rtManager ? mDrawArgs->rtManager->ResY() : 1)
	};
	
	cmdCtx.PushConstants(0, (*blurry ? 6 : 4) * sizeof(float), pcData);
	
	if (*blurry)
	{
		cmdCtx.BindTexture(mDrawArgs->glassBlurTexture, 0, 2);
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindTexture(mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 0, 2);
	}
	else
	{
		cmdCtx.BindTexture(blackPixelTexture, 0, 2);
	}
	
	return true;
}

bool BlurredGlassMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
