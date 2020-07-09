#include "GlassBlurRenderer.hpp"

GlassBlurRenderer::GlassBlurRenderer()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Post.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GlassBlur.fs.glsl").DefaultVariant();
	pipelineCI.label = "GlassBlurPipeline";
	m_blurPipeline = eg::Pipeline::Create(pipelineCI);
}

void GlassBlurRenderer::MaybeUpdateResolution(uint32_t newWidth, uint32_t newHeight)
{
	if (newWidth == m_inputWidth && newHeight == m_inputHeight)
		return;
	m_inputWidth = newWidth;
	m_inputHeight = newHeight;
	
	m_blurTextureOut.Destroy();
	m_blurTextureTmp.Destroy();
	for (uint32_t i = 0; i < LEVELS; i++)
	{
		m_framebuffersOut[i].Destroy();
		m_framebuffersTmp[i].Destroy();
	}
	
	eg::SamplerDescription samplerDesc;
	samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	samplerDesc.minFilter = eg::TextureFilter::Linear;
	samplerDesc.magFilter = eg::TextureFilter::Linear;
	
	eg::TextureCreateInfo textureCI;
	textureCI.width = newWidth / 2;
	textureCI.height = newHeight / 2;
	textureCI.mipLevels = LEVELS;
	textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::ManualBarrier;
	textureCI.format = eg::Format::R16G16B16A16_Float;
	textureCI.label = "GlassBlurDst";
	textureCI.defaultSamplerDescription = &samplerDesc;
	
	m_blurTextureTmp = eg::Texture::Create2D(textureCI);
	m_blurTextureOut = eg::Texture::Create2D(textureCI);
	
	for (uint32_t i = 0; i < LEVELS; i++)
	{
		eg::FramebufferAttachment colorAttachment;
		colorAttachment.subresource.mipLevel = i;
		
		colorAttachment.texture = m_blurTextureTmp.handle;
		m_framebuffersTmp[i] = eg::Framebuffer({ &colorAttachment, 1 });
		
		colorAttachment.texture = m_blurTextureOut.handle;
		m_framebuffersOut[i] = eg::Framebuffer({ &colorAttachment, 1 });
	}
}

void GlassBlurRenderer::DoBlurPass(const glm::vec2& blurVector, eg::TextureRef inputTexture,
                                   int inputLod, eg::FramebufferRef dstFramebuffer) const
{
	eg::RenderPassBeginInfo rp1BeginInfo;
	rp1BeginInfo.framebuffer = dstFramebuffer.handle;
	rp1BeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rp1BeginInfo);
	
	eg::DC.BindPipeline(m_blurPipeline);
	
	eg::TextureSubresource subresource;
	subresource.firstMipLevel = inputLod;
	subresource.numMipLevels = 1;
	eg::DC.BindTexture(inputTexture, 0, 0, nullptr, subresource);
	
	eg::DC.PushConstants(0, sizeof(float) * 2, &blurVector.x);
	
	eg::DC.Draw(0, 3, 0, 1);
	
	eg::DC.EndRenderPass();
}

static inline void ChangeUsageForShaderSample(eg::TextureRef texture, int mipLevel)
{
	eg::TextureBarrier barrier;
	barrier.oldAccess = eg::ShaderAccessFlags::None;
	barrier.newAccess = eg::ShaderAccessFlags::Fragment;
	barrier.oldUsage = eg::TextureUsage::FramebufferAttachment;
	barrier.newUsage = eg::TextureUsage::ShaderSample;
	barrier.subresource.firstMipLevel = mipLevel;
	barrier.subresource.numMipLevels = 1;
	eg::DC.Barrier(texture, barrier);
}

void GlassBlurRenderer::Render(eg::TextureRef inputTexture) const
{
	glm::vec2 pixelSize = 1.0f / glm::vec2(m_inputWidth, m_inputHeight);
	
	for (int i = 0; i < LEVELS; i++)
	{
		DoBlurPass(glm::vec2(pixelSize.x, 0), i != 0 ? eg::TextureRef(m_blurTextureOut) : inputTexture,
			std::max(i - 1, 0), m_framebuffersTmp[i]);
		ChangeUsageForShaderSample(m_blurTextureTmp, i);
		
		pixelSize *= 2.0f;
		
		DoBlurPass(glm::vec2(0, pixelSize.y), m_blurTextureTmp, i, m_framebuffersOut[i]);
		ChangeUsageForShaderSample(m_blurTextureOut, i);
	}
}
