#pragma once

#include "RenderSettings.hpp"
#include "QualityLevel.hpp"

#include <functional>

class PlanarReflectionsManager
{
public:
	PlanarReflectionsManager();
	
	void SetQuality(QualityLevel qualityLevel);
	
	void ResolutionChanged();
	
	void BeginFrame();
	
	using RenderCallback = std::function<void(const RenderSettings&, eg::FramebufferRef framebuffer)>;
	
	uint32_t RenderPlanarReflections(const eg::Plane& plane, const RenderSettings& realRenderSettings,
		const RenderCallback& renderCallback);
	
private:
	inline bool LitReflections()
	{
		return m_qualityLevel >= QualityLevel::High;
	}
	
	struct RenderTexture
	{
		eg::Texture texture;
		eg::Framebuffer framebuffer;
	};
	
	std::vector<RenderTexture> m_reflectionTextures;
	uint32_t m_numUsedReflectionTextures = 0;
	
	eg::Texture m_depthTexture;
	
	QualityLevel m_qualityLevel;
	eg::Format m_textureFormat;
	uint32_t m_textureWidth;
	uint32_t m_textureHeight;
	
	RenderSettings m_renderSettings;
};
