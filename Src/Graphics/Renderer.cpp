#include "Renderer.hpp"

Renderer::Renderer()
{
	eg::ShaderProgram postProgram;
	postProgram.AddStageFromAsset("Shaders/Post.vs.glsl");
	postProgram.AddStageFromAsset("Shaders/Post.fs.glsl");
	
	eg::FixedFuncState ffState;
	ffState.depthFormat = eg::Format::DefaultDepthStencil;
	ffState.attachments[0].format = eg::Format::DefaultColor;
	m_postPipeline = postProgram.CreatePipeline(ffState);
	
	eg::SamplerDescription attachmentSamplerDesc;
	attachmentSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	attachmentSamplerDesc.minFilter = eg::TextureFilter::Nearest;
	attachmentSamplerDesc.magFilter = eg::TextureFilter::Nearest;
	attachmentSamplerDesc.mipFilter = eg::TextureFilter::Nearest;
	m_attachmentSampler = eg::Sampler(attachmentSamplerDesc);
}

void Renderer::Begin(const eg::ColorLin* clearColor)
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
		colorTexCreateInfo.format = COLOR_FORMAT;
		m_colorTexture = eg::Texture::Create2D(colorTexCreateInfo);
		
		eg::Texture2DCreateInfo depthTexCreateInfo;
		depthTexCreateInfo.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		depthTexCreateInfo.width = (uint32_t)m_resX;
		depthTexCreateInfo.height = (uint32_t)m_resY;
		depthTexCreateInfo.mipLevels = 1;
		depthTexCreateInfo.format = DEPTH_FORMAT;
		m_depthTexture = eg::Texture::Create2D(depthTexCreateInfo);
		
		m_mainFramebuffer = eg::Framebuffer({ &m_colorTexture, 1 }, m_depthTexture);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_mainFramebuffer.handle;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	if (clearColor != nullptr)
	{
		rpBeginInfo.colorAttachments[0].clearValue = *clearColor;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	}
	else
	{
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	}
	eg::DC.BeginRenderPass(rpBeginInfo);
}

void Renderer::End()
{
	eg::DC.EndRenderPass();
	
	m_colorTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = nullptr;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	eg::DC.BindPipeline(m_postPipeline);
	
	eg::DC.BindTexture(m_colorTexture, 0, &m_attachmentSampler);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}
