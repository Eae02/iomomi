#include "Renderer.hpp"

Renderer::Renderer()
{
	eg::ShaderProgram postProgram;
	postProgram.AddStageFromAsset("Shaders/Post.vs.glsl");
	postProgram.AddStageFromAsset("Shaders/Post.fs.glsl");
	
	eg::FixedFuncState postFFState;
	postFFState.depthFormat = eg::Format::DefaultDepthStencil;
	postFFState.attachments[0].format = eg::Format::DefaultColor;
	m_postPipeline = postProgram.CreatePipeline(postFFState);
	
	eg::ShaderProgram ambientProgram;
	ambientProgram.AddStageFromAsset("Shaders/Post.vs.glsl");
	ambientProgram.AddStageFromAsset("Shaders/DeferredBase.fs.glsl");
	
	eg::FixedFuncState ambientFFState;
	ambientFFState.attachments[0].format = LIGHT_COLOR_FORMAT;
	m_ambientPipeline = ambientProgram.CreatePipeline(ambientFFState);
	
	eg::SamplerDescription attachmentSamplerDesc;
	attachmentSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.minFilter = eg::TextureFilter::Nearest;
	attachmentSamplerDesc.magFilter = eg::TextureFilter::Nearest;
	attachmentSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
	m_attachmentSampler = eg::Sampler(attachmentSamplerDesc);
}

void Renderer::BeginGeometry()
{
	if (m_resX != eg::CurrentResolutionX() || m_resY != eg::CurrentResolutionY())
	{
		m_resX = eg::CurrentResolutionX();
		m_resY = eg::CurrentResolutionY();
		
		eg::Texture2DCreateInfo colorTexCreateInfo;
		colorTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		colorTexCreateInfo.width = (uint32_t)m_resX;
		colorTexCreateInfo.height = (uint32_t)m_resY;
		colorTexCreateInfo.mipLevels = 1;
		colorTexCreateInfo.format = eg::Format::R8G8B8A8_UNorm;
		m_gbColor1Texture = eg::Texture::Create2D(colorTexCreateInfo);
		m_gbColor2Texture = eg::Texture::Create2D(colorTexCreateInfo);
		
		eg::Texture2DCreateInfo depthTexCreateInfo;
		depthTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		depthTexCreateInfo.width = (uint32_t)m_resX;
		depthTexCreateInfo.height = (uint32_t)m_resY;
		depthTexCreateInfo.mipLevels = 1;
		depthTexCreateInfo.format = DEPTH_FORMAT;
		m_gbDepthTexture = eg::Texture::Create2D(depthTexCreateInfo);
		
		const eg::TextureRef gbTextures[] = { m_gbColor1Texture, m_gbColor2Texture };
		m_gbFramebuffer = eg::Framebuffer(gbTextures, m_gbDepthTexture);
		
		eg::Texture2DCreateInfo lightOutTexCreateInfo;
		lightOutTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		lightOutTexCreateInfo.width = (uint32_t)m_resX;
		lightOutTexCreateInfo.height = (uint32_t)m_resY;
		lightOutTexCreateInfo.mipLevels = 1;
		lightOutTexCreateInfo.format = LIGHT_COLOR_FORMAT;
		m_lightOutTexture = eg::Texture::Create2D(lightOutTexCreateInfo);
		
		m_lightOutFramebuffer = eg::Framebuffer({ &m_lightOutTexture, 1 });
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_gbFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	rpBeginInfo.colorAttachments[1].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void Renderer::BeginLighting()
{
	eg::DC.EndRenderPass();
	
	m_gbColor1Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	//m_gbColor2Texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	//m_gbDepthTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_lightOutFramebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_ambientPipeline);
	
	eg::DC.BindTexture(m_gbColor1Texture, 0, &m_attachmentSampler);
	
	float ambient[] = { 0.6f, 0.5f, 0.5f };
	eg::DC.PushConstants(0, sizeof(ambient), ambient);
	
	eg::DC.Draw(0, 3, 0, 1);
}

void Renderer::End()
{
	eg::DC.EndRenderPass();
	
	m_lightOutTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_postPipeline);
	
	eg::DC.BindTexture(m_lightOutTexture, 0, &m_attachmentSampler);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
