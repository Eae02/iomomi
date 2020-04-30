#include "PostProcessor.hpp"
#include "../Settings.hpp"

static float* bloomIntensity = eg::TweakVarFloat("bloom_intensity", 0.5f, 0);

PostProcessor::PostProcessor()
{
	eg::SamplerDescription samplerDescription;
	samplerDescription.wrapU = eg::WrapMode::ClampToEdge;
	samplerDescription.wrapV = eg::WrapMode::ClampToEdge;
	samplerDescription.wrapW = eg::WrapMode::ClampToEdge;
	m_inputSampler = eg::Sampler(samplerDescription);
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

void PostProcessor::Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget)
{
	if (m_bloomWasEnabled != settings.BloomEnabled() || m_fxaaWasEnabled != settings.enableFXAA)
	{
		InitPipeline();
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	const float pc[] = {
		1.0f / eg::CurrentResolutionX(),
		1.0f / eg::CurrentResolutionY(),
		settings.exposure,
		*bloomIntensity
	};
	
	eg::DC.BindPipeline(m_pipeline);
	eg::DC.PushConstants(0, sizeof(float) * 4, pc);
	
	if (bloomRenderTarget != nullptr)
	{
		eg::DC.BindTexture(bloomRenderTarget->OutputTexture(), 0, 1, &m_inputSampler);
	}
	
	eg::DC.BindTexture(input, 0, 0, &m_inputSampler);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	
	const float CROSSHAIR_SIZE = 32;
	const float CROSSHAIR_OPACITY = 0.75f;
	eg::SpriteBatch::overlay.Draw(eg::GetAsset<eg::Texture>("Textures/Crosshair.png"),
	    eg::Rectangle::CreateCentered(eg::CurrentResolutionX() / 2.0f, eg::CurrentResolutionY() / 2.0f, CROSSHAIR_SIZE, CROSSHAIR_SIZE),
	    eg::ColorLin(eg::Color::White.ScaleAlpha(CROSSHAIR_OPACITY)), eg::SpriteFlags::None);
}
