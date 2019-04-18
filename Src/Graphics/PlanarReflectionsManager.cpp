#include "PlanarReflectionsManager.hpp"

PlanarReflectionsManager::PlanarReflectionsManager()
{
	SetQuality(QualityLevel::Off);
}

void PlanarReflectionsManager::BeginFrame()
{
	m_numUsedReflectionTextures = 0;
}

void PlanarReflectionsManager::RenderPlanarReflections(ReflectionPlane& plane,
	const PlanarReflectionsManager::RenderCallback& renderCallback)
{
	if (m_qualityLevel == QualityLevel::Off)
		return;
	
	if (m_depthTexture.handle == nullptr)
	{
		eg::Texture2DCreateInfo depthTextureCI;
		depthTextureCI.format = eg::Format::Depth16;
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
		
		eg::Texture2DCreateInfo textureCI;
		textureCI.format = m_textureFormat;
		textureCI.width = m_textureWidth;
		textureCI.height = m_textureHeight;
		textureCI.mipLevels = 1;
		textureCI.flags = eg::TextureFlags::FramebufferAttachment | eg::TextureFlags::ShaderSample;
		textureCI.defaultSamplerDescription = &samplerDescription;
		
		eg::Texture texture = eg::Texture::Create2D(textureCI);
		
		eg::FramebufferAttachment colorAttachment(texture.handle);
		eg::FramebufferAttachment depthAttachment(m_depthTexture.handle);
		eg::Framebuffer framebuffer({ &colorAttachment, 1 }, depthAttachment);
		
		m_reflectionTextures.push_back({ std::move(texture), std::move(framebuffer) });
	}
	
	renderCallback(plane, m_reflectionTextures[m_numUsedReflectionTextures].framebuffer);
	
	plane.texture = m_reflectionTextures[m_numUsedReflectionTextures].texture;
	plane.texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	m_numUsedReflectionTextures++;
}

void PlanarReflectionsManager::SetQuality(QualityLevel qualityLevel)
{
	m_qualityLevel = qualityLevel;
	m_textureFormat = qualityLevel >= QualityLevel::High ? eg::Format::R16G16B16A16_Float: eg::Format::R8G8B8A8_UNorm;
	ResolutionChanged();
}

void PlanarReflectionsManager::ResolutionChanged()
{
	int textureDiv;
	if (m_qualityLevel <= QualityLevel::Low)
		textureDiv = 4;
	else if (m_qualityLevel <= QualityLevel::Medium)
		textureDiv = 2;
	else
		textureDiv = 1;
	
	m_textureWidth = eg::CurrentResolutionX() / textureDiv;
	m_textureHeight = eg::CurrentResolutionY() / textureDiv;
	m_reflectionTextures.clear();
	m_depthTexture = { };
}
