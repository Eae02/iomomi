#include "PlanarReflectionsManager.hpp"

void PlanarReflectionsManager::BeginFrame()
{
	m_numUsedReflectionTextures = 0;
}

uint32_t PlanarReflectionsManager::RenderPlanarReflections(const eg::Plane& plane)
{
	if (m_numUsedReflectionTextures >= m_reflectionTextures.size())
	{
		eg::Texture2DCreateInfo textureCI;
		textureCI.format = m_textureFormat;
		textureCI.width = m_textureWidth;
		textureCI.height = m_textureHeight;
		
		m_reflectionTextures.emplace_back(eg::Texture::Create2D());
	}
	
	const uint32_t id = m_numUsedReflectionTextures;
	
	
	
	m_numUsedReflectionTextures++;
	return id;
}

void PlanarReflectionsManager::SetQuality(QualityLevel qualityLevel)
{
	m_qualityLevel = qualityLevel;
	m_textureFormat = eg::Format::R8G8B8A8_UNorm;
	ResolutionChanged();
}

void PlanarReflectionsManager::ResolutionChanged()
{
	m_textureWidth = eg::CurrentResolutionX() / 2;
	m_textureHeight = eg::CurrentResolutionY() / 2;
	m_reflectionTextures.clear();
}
