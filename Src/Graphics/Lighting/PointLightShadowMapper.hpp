#pragma once

#include "PointLight.hpp"

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
	
	void SetResolution(uint32_t resolution);
	
	void Invalidate(const eg::Sphere& sphere);
	
	void UpdateShadowMaps(std::vector<PointLightDrawData>& pointLights, const RenderCallback& renderCallback);
	
	static constexpr eg::Format SHADOW_MAP_FORMAT = eg::Format::Depth16;
	static constexpr uint32_t BUFFER_SIZE = sizeof(glm::mat4) * 6 + sizeof(float) * 4;
	
private:
	size_t AddShadowMap();
	
	void UpdateShadowMap(size_t index, const RenderCallback& renderCallback);
	
	uint32_t m_resolution = 256;
	uint32_t m_maxUpdatesPerFrame = 2;
	
	struct ShadowMap
	{
		eg::Texture texture;
		eg::Framebuffer framebuffer;
		eg::Sphere sphere;
		uint64_t currentLightID;
		bool outOfDate;
		bool inUse;
	};
	
	std::vector<ShadowMap> m_shadowMaps;
	
	std::vector<size_t> m_updateCandidates;
	
	eg::Buffer m_matricesBuffer;
};
