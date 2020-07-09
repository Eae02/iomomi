#include "BlurredGlassMaterial.hpp"
#include "StaticPropMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"

static eg::Pipeline pipelineBlurry;
static eg::Pipeline pipelineNotBlurry;

static void OnInit()
{
	const eg::ShaderModuleAsset& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/BlurredGlass.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.topology = eg::Topology::TriangleList;
	
	
	pipelineCI.label = "BlurredGlass[Blurry]";
	pipelineCI.fragmentShader = fs.GetVariant("VBlur");
	pipelineBlurry = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFunc::Add,
		/* srcColor */ eg::BlendFactor::Zero,
		/* srcAlpha */ eg::BlendFactor::Zero,
		/* dstColor */ eg::BlendFactor::SrcColor,
		/* dstAlpha */ eg::BlendFactor::One);
	pipelineCI.label = "BlurredGlass[NotBlurry]";
	pipelineCI.fragmentShader = fs.GetVariant("VNoBlur");
	pipelineNotBlurry = eg::Pipeline::Create(pipelineCI);
}

static void OnShutdown()
{
	pipelineBlurry.Destroy();
	pipelineNotBlurry.Destroy();
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
	if (meshDrawArgs.drawMode == MeshDrawMode::TransparentAfterWater)
		return isBlurry && meshDrawArgs.glassBlurTexture.handle != nullptr;
	return { };
}

float* clearGlassHexAlpha = eg::TweakVarFloat("glass_hex_alpha", 0.8f, 0.0f, 1.0f);

bool BlurredGlassMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	const MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
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
		1.0f / eg::CurrentResolutionX(),
		1.0f / eg::CurrentResolutionY()
	};
	
	cmdCtx.PushConstants(0, (*blurry ? 6 : 4) * sizeof(float), pcData);
	
	if (*blurry)
	{
		cmdCtx.BindTexture(mDrawArgs->glassBlurTexture, 0, 2);
	}
	
	return true;
}

bool BlurredGlassMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
