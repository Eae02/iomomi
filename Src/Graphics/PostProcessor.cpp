#include "PostProcessor.hpp"
#include "../Settings.hpp"

PostProcessor::PostProcessor()
{
	const eg::ShaderModuleAsset& fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.fs.glsl");
	
	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	postPipelineCI.fragmentShader = fragmentShader.GetVariant("VNoBloom");
	postPipelineCI.label = "PostProcess";
	m_pipelineNoBloom = eg::Pipeline::Create(postPipelineCI);
	m_pipelineNoBloom.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	postPipelineCI.fragmentShader = fragmentShader.GetVariant("VBloom");
	m_pipelineBloom = eg::Pipeline::Create(postPipelineCI);
	m_pipelineBloom.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	eg::SamplerDescription samplerDescription;
	samplerDescription.wrapU = eg::WrapMode::ClampToEdge;
	samplerDescription.wrapV = eg::WrapMode::ClampToEdge;
	samplerDescription.wrapW = eg::WrapMode::ClampToEdge;
	m_inputSampler = eg::Sampler(samplerDescription);
}

void PostProcessor::Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget)
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	if (bloomRenderTarget != nullptr)
	{
		eg::DC.BindPipeline(m_pipelineBloom);
		eg::DC.BindTexture(bloomRenderTarget->OutputTexture(), 0, 1, &m_inputSampler);
		
		float pc[] = { settings.exposure, bloomIntensity };
		eg::DC.PushConstants(0, sizeof(pc), pc);
	}
	else
	{
		eg::DC.BindPipeline(m_pipelineNoBloom);
		
		float pc[] = { settings.exposure };
		eg::DC.PushConstants(0, sizeof(pc), pc);
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
