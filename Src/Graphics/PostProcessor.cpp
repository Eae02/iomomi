#include "PostProcessor.hpp"
#include "../Settings.hpp"
#include "GraphicsCommon.hpp"

static float* bloomIntensity = eg::TweakVarFloat("bloom_intensity", 0.4f, 0);

static float* vignetteMinRad = eg::TweakVarFloat("vign_min_rad", 0.2f, 0);
static float* vignetteMaxRad = eg::TweakVarFloat("vign_max_rad", 0.75f, 0);
static float* vignettePower = eg::TweakVarFloat("vign_power", 1.2f, 0);

void PostProcessor::InitPipeline(bool bloomEnabled)
{
	struct SpecConstData
	{
		uint32_t enableBloom;
		uint32_t enableFXAA;
	};
	SpecConstData specConstData;
	specConstData.enableBloom = bloomEnabled;
	specConstData.enableFXAA = settings.enableFXAA;
	
	eg::SpecializationConstantEntry specConstEntries[2];
	specConstEntries[0].constantID = 0;
	specConstEntries[0].size = sizeof(uint32_t);
	specConstEntries[0].offset = offsetof(SpecConstData, enableBloom);
	specConstEntries[1].constantID = 1;
	specConstEntries[1].size = sizeof(uint32_t);
	specConstEntries[1].offset = offsetof(SpecConstData, enableFXAA);
	
	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	postPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.fs.glsl").DefaultVariant();
	postPipelineCI.label = "PostProcess";
	postPipelineCI.fragmentShader.specConstants = specConstEntries;
	postPipelineCI.fragmentShader.specConstantsData = &specConstData;
	postPipelineCI.fragmentShader.specConstantsDataSize = sizeof(SpecConstData);
	m_pipeline = eg::Pipeline::Create(postPipelineCI);
	m_pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	m_bloomWasEnabled = bloomEnabled;
	m_fxaaWasEnabled = settings.enableFXAA;
}

void PostProcessor::Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget,
	eg::FramebufferHandle output, uint32_t outputResX, uint32_t outputResY, float colorScale)
{
	bool bloomEnabled = bloomRenderTarget != nullptr;
	if (m_bloomWasEnabled != bloomEnabled || m_fxaaWasEnabled != settings.enableFXAA)
	{
		InitPipeline(bloomEnabled);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = output;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	const float pc[] =
	{
		settings.exposure,
		*bloomIntensity,
		colorScale,
		*vignetteMinRad,
		1.0f / (*vignetteMaxRad - *vignetteMinRad),
		*vignettePower
	};
	
	eg::DC.BindPipeline(m_pipeline);
	eg::DC.PushConstants(0, sizeof(pc), pc);
	
	eg::DC.BindTexture(input, 0, 0, &framebufferLinearSampler);
	
	eg::TextureRef bloomTexture = bloomRenderTarget ? bloomRenderTarget->OutputTexture() : blackPixelTexture;
	eg::DC.BindTexture(bloomTexture, 0, 1, &framebufferLinearSampler);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
