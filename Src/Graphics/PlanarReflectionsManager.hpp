#pragma once

#include "RenderSettings.hpp"
#include "QualityLevel.hpp"

class PlanarReflectionsManager
{
public:
	
	void SetQuality(QualityLevel qualityLevel);
	
	void ResolutionChanged();
	
	void BeginFrame();
	
	uint32_t RenderPlanarReflections(const eg::Plane& plane);
	
private:
	inline bool LitReflections()
	{
		return m_qualityLevel >= QualityLevel::High;
	}
	
	std::vector<eg::Texture> m_reflectionTextures;
	uint32_t m_numUsedReflectionTextures = 0;
	
	eg::Texture m_depthBuffer;
	
	QualityLevel m_qualityLevel;
	eg::Format m_textureFormat;
	uint32_t m_textureWidth;
	uint32_t m_textureHeight;
	
	RenderSettings m_renderSettings;
};
