#include "PostProcessor.hpp"
#include "../Settings.hpp"
#include "GraphicsCommon.hpp"

static float* bloomIntensity = eg::TweakVarFloat("bloom_intensity", 0.5f, 0);

PostProcessor::PostProcessor()
{
	
}

void PostProcessor::InitPipeline()
{
	struct SpecConstData
	{
		uint32_t enableBloom;
		uint32_t enableFXAA;
	};
	SpecConstData specConstData;
	specConstData.enableBloom = settings.BloomEnabled();
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
	
	m_bloomWasEnabled = settings.BloomEnabled();
	m_fxaaWasEnabled = settings.enableFXAA;
}

void PostProcessor::Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget,
	eg::FramebufferHandle output, uint32_t outputResX, uint32_t outputResY, float colorScale)
{
	if (m_bloomWasEnabled != settings.BloomEnabled() || m_fxaaWasEnabled != settings.enableFXAA)
	{
		InitPipeline();
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = output;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	const float pc[] = {
		1.0f / outputResX,
		1.0f / outputResY,
		settings.exposure,
		*bloomIntensity,
		colorScale
	};
	
	eg::DC.BindPipeline(m_pipeline);
	eg::DC.PushConstants(0, sizeof(pc), pc);
	
	eg::DC.BindTexture(input, 0, 0, &framebufferLinearSampler);
	
	eg::TextureRef bloomTexture = bloomRenderTarget ? bloomRenderTarget->OutputTexture() : blackPixelTexture;
	eg::DC.BindTexture(bloomTexture, 0, 1, &framebufferLinearSampler);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
