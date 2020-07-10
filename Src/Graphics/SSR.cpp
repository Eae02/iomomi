#include "SSR.hpp"
#include "GraphicsCommon.hpp"
#include "RenderSettings.hpp"
#include "../Settings.hpp"

//Stores a pair of two SSR sample counts (for the linear search and the binary search respectively)
// for each each reflection quality level. 0 means SSR is disabled
std::pair<uint32_t, uint32_t> SSR_SAMPLES[] = 
{
	/* VeryLow  */ { 0, 0 },
	/* Low      */ { 4, 4 },
	/* Medium   */ { 4, 6 },
	/* High     */ { 6, 6 },
	/* VeryHigh */ { 8, 8 }
};

void SSR::CreatePipeline()
{
	eg::SpecializationConstantEntry specConstEntries[2];
	specConstEntries[0].constantID = 0;
	specConstEntries[0].size = sizeof(uint32_t);
	specConstEntries[0].offset = offsetof(std::decay_t<decltype(SSR_SAMPLES[0])>, first);
	specConstEntries[1].constantID = 1;
	specConstEntries[1].size = sizeof(uint32_t);
	specConstEntries[1].offset = offsetof(std::decay_t<decltype(SSR_SAMPLES[0])>, second);
	
	eg::GraphicsPipelineCreateInfo ambientPipelineCI;
	ambientPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	ambientPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/SSR.fs.glsl").DefaultVariant();
	ambientPipelineCI.fragmentShader.specConstants = specConstEntries;
	ambientPipelineCI.fragmentShader.specConstantsData = &SSR_SAMPLES[(int)settings.reflectionsQuality];
	ambientPipelineCI.fragmentShader.specConstantsDataSize = sizeof(SSR_SAMPLES[0]);
	m_pipeline = eg::Pipeline::Create(ambientPipelineCI);
	m_pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR);
	m_pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR);
}

void SSR::Render(eg::TextureRef waterDepth, RenderTex destinationTexture)
{
	if (settings.reflectionsQuality != m_currentReflectionQualityLevel)
	{
		CreatePipeline();
		m_currentReflectionQualityLevel = settings.reflectionsQuality;
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = GetFramebuffer(destinationTexture, {}, {}, "SSR");
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_pipeline);
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::LitWithoutSSR), 0, 1);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor1), 0, 2);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBColor2), 0, 3);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::GBDepth), 0, 4);
	eg::DC.BindTexture(GetRenderTexture(RenderTex::Flags), 0, 5);
	eg::DC.BindTexture(waterDepth, 0, 6);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
