#include "PostProcessor.hpp"

static constexpr float EXPOSURE = 1.4f;
static constexpr float BLOOM_INTENSITY = 0.5f;

PostProcessor::PostProcessor()
{
	eg::GraphicsPipelineCreateInfo postPipelineCI;
	postPipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	postPipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.fs.glsl").DefaultVariant();
	postPipelineCI.label = "PostProcess";
	m_postPipeline = eg::Pipeline::Create(postPipelineCI);
	m_postPipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	eg::SamplerDescription samplerDescription;
	samplerDescription.wrapU = eg::WrapMode::ClampToEdge;
	samplerDescription.wrapV = eg::WrapMode::ClampToEdge;
	samplerDescription.wrapW = eg::WrapMode::ClampToEdge;
	m_inputSampler = eg::Sampler(samplerDescription);
}

void PostProcessor::Render(eg::TextureRef input, eg::TextureRef bloomTexture)
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_postPipeline);
	
	eg::DC.BindTexture(input, 0, 0, &m_inputSampler);
	eg::DC.BindTexture(bloomTexture, 0, 1, &m_inputSampler);
	
	float pc[] = { EXPOSURE, BLOOM_INTENSITY };
	eg::DC.PushConstants(0, sizeof(pc), pc);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
	
	const float CROSSHAIR_SIZE = 32;
	const float CROSSHAIR_OPACITY = 0.75f;
	eg::SpriteBatch::overlay.Draw(eg::GetAsset<eg::Texture>("Textures/Crosshair.png"),
	    eg::Rectangle::CreateCentered(eg::CurrentResolutionX() / 2.0f, eg::CurrentResolutionY() / 2.0f, CROSSHAIR_SIZE, CROSSHAIR_SIZE),
	    eg::ColorLin(eg::Color::White.ScaleAlpha(CROSSHAIR_OPACITY)), eg::FlipFlags::Normal);
}
