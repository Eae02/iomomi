#include "PlanarReflectionsManager.hpp"

PlanarReflectionsManager::PlanarReflectionsManager()
{
	SetQuality(QualityLevel::Medium);
}

void PlanarReflectionsManager::BeginFrame()
{
	m_numUsedReflectionTextures = 0;
}

void PlanarReflectionsManager::RenderPlanarReflections(ReflectionPlane& plane,
	const PlanarReflectionsManager::RenderCallback& renderCallback)
{
	if (m_qualityLevel == QualityLevel::VeryLow)
		return;
	
	if (m_depthTexture.handle == nullptr)
	{
		eg::TextureCreateInfo depthTextureCI;
		depthTextureCI.format = eg::Format::Depth32;
		depthTextureCI.width = m_textureWidth;
		depthTextureCI.height = m_textureHeight;
		depthTextureCI.mipLevels = 1;
		depthTextureCI.flags = eg::TextureFlags::FramebufferAttachment;
		
		m_depthTexture = eg::Texture::Create2D(depthTextureCI);
	}
	
	if (m_numUsedReflectionTextures >= m_reflectionTextures.size())
	{
		eg::SamplerDescription samplerDescription;
		samplerDescription.wrapU = eg::WrapMode::ClampToEdge;
		samplerDescription.wrapV = eg::WrapMode::ClampToEdge;
		samplerDescription.wrapW = eg::WrapMode::ClampToEdge;
		
		eg::TextureCreateInfo textureCI;
		textureCI.format = m_textureFormat;
		textureCI.width = m_textureWidth;
		textureCI.height = m_textureHeight;
		textureCI.mipLevels = TEXTURE_LEVELS;
		textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample | eg::TextureFlags::ManualBarrier;
		textureCI.defaultSamplerDescription = &samplerDescription;
		
		RenderTexture& renderTexture = m_reflectionTextures.emplace_back();
		renderTexture.texture = eg::Texture::Create2D(textureCI);
		
		eg::FramebufferAttachment colorAttachment(renderTexture.texture.handle);
		colorAttachment.subresource.mipLevel = 0;
		eg::FramebufferAttachment depthAttachment(m_depthTexture.handle);
		renderTexture.framebuffers[0] = eg::Framebuffer({ &colorAttachment, 1 }, depthAttachment);
		
		for (uint32_t i = 1; i < TEXTURE_LEVELS; i++)
		{
			colorAttachment.subresource.mipLevel = i;
			renderTexture.framebuffers[i] = eg::Framebuffer({ &colorAttachment, 1 });
		}
	}
	
	RenderTexture& texture = m_reflectionTextures[m_numUsedReflectionTextures];
	
	renderCallback(plane, texture.framebuffers[0]);
	
	eg::TextureBarrier barrier;
	barrier.oldAccess = eg::ShaderAccessFlags::None;
	barrier.newAccess = eg::ShaderAccessFlags::Fragment;
	barrier.oldUsage = eg::TextureUsage::FramebufferAttachment;
	barrier.newUsage = eg::TextureUsage::ShaderSample;
	barrier.subresource.firstMipLevel = 0;
	barrier.subresource.numMipLevels = 1;
	eg::DC.Barrier(texture.texture, barrier);
	
	plane.texture = texture.texture;
	
	if (plane.blur)
	{
		
	}
	
	m_numUsedReflectionTextures++;
}

void PlanarReflectionsManager::SetQuality(QualityLevel qualityLevel)
{
	if (m_qualityLevel == qualityLevel)
		return;
	m_qualityLevel = qualityLevel;
	m_textureFormat = qualityLevel >= QualityLevel::High ? eg::Format::R16G16B16A16_Float : eg::Format::R8G8B8A8_UNorm;
	
	ResolutionChanged();
}

void PlanarReflectionsManager::ResolutionChanged()
{
	int textureDiv;
	if (m_qualityLevel <= QualityLevel::Low)
		textureDiv = 4;
	else if (m_qualityLevel <= QualityLevel::High)
		textureDiv = 2;
	else
		textureDiv = 1;
	
	m_textureWidth = eg::CurrentResolutionX() / textureDiv;
	m_textureHeight = eg::CurrentResolutionY() / textureDiv;
	m_reflectionTextures.clear();
	m_depthTexture = { };
}
