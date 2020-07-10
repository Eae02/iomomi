#pragma once

#include "PointLight.hpp"
#include "../QualityLevel.hpp"

struct PointLightShadowRenderArgs
{
	eg::Sphere lightSphere;
	eg::BufferRef matricesBuffer;
};

class PointLightShadowMapper
{
public:
	using RenderCallback = std::function<void(const PointLightShadowRenderArgs&)>;
	
	PointLightShadowMapper();
	
	void SetQuality(QualityLevel quality);
	
	void Invalidate(const eg::Sphere& sphere);
	
	void InvalidateAll();
	
	void UpdateShadowMaps(std::vector<PointLightDrawData>& pointLights, const RenderCallback& renderCallback);
	
	uint64_t LastUpdateFrameIndex() const { return m_lastUpdateFrameIndex; }
	uint64_t LastFrameUpdateCount() const { return m_lastFrameUpdateCount; }
	
	static constexpr eg::Format SHADOW_MAP_FORMAT = eg::Format::Depth16;
	static constexpr uint32_t BUFFER_SIZE = sizeof(glm::mat4) * 6 + sizeof(float) * 4;
	
private:
	size_t AddShadowMap();
	
	void UpdateShadowMap(size_t index, const RenderCallback& renderCallback);
	
	QualityLevel m_qualityLevel = QualityLevel::Medium;
	uint32_t m_resolution = 256;
	
	uint64_t m_lastUpdateFrameIndex = 0;
	uint64_t m_lastFrameUpdateCount = 0;
	
	struct ShadowMap
	{
		eg::Texture texture;
		eg::Framebuffer framebuffer;
		eg::Sphere sphere;
		uint64_t currentLightID;
		uint64_t lastUpdateFrame;
		bool outOfDate;
		bool inUse;
	};
	
	std::vector<ShadowMap> m_shadowMaps;
	
	eg::Buffer m_matricesBuffer;
};
