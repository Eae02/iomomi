#pragma once

#include "../QualityLevel.hpp"
#include "PointLight.hpp"

class PointLightShadowMapper
{
public:
	using RenderCallback = std::function<void(const PointLightShadowDrawArgs&)>;

	PointLightShadowMapper();

	void SetQuality(QualityLevel quality);

	void Invalidate(const eg::Sphere& sphere, const PointLight* onlyThisLight = nullptr);

	void SetLightSources(const std::vector<std::shared_ptr<PointLight>>& lights);

	void UpdateShadowMaps(
		const RenderCallback& prepareCallback, const RenderCallback& renderCallback, const eg::Frustum& viewFrustum);

	static bool FlippedLightMatrix();

	uint32_t Resolution() const { return m_resolution; }

	uint64_t LastUpdateFrameIndex() const { return m_lastUpdateFrameIndex; }
	uint64_t LastFrameUpdateCount() const { return m_lastFrameUpdateCount; }

	static constexpr eg::Format SHADOW_MAP_FORMAT = eg::Format::Depth16;
	static constexpr uint32_t BUFFER_SIZE = sizeof(glm::mat4) * 6 + sizeof(float) * 4;

private:
	struct ShadowMap
	{
		eg::Texture texture;
		eg::Framebuffer framebuffers[6];
		eg::TextureViewHandle wholeTextureView;
		eg::TextureViewHandle layerViews[6];
		bool inUse;
	};

	struct LightEntry
	{
		std::shared_ptr<PointLight> light;
		int staticShadowMap;
		int dynamicShadowMap;
		bool faceInvalidated[6];
		glm::vec3 previousPosition;
		float previousRange;

		bool HasMoved() const;
	};

	int AllocateShadowMap();

	QualityLevel m_qualityLevel = QualityLevel::Medium;
	uint32_t m_resolution = 256;

	uint64_t m_lastUpdateFrameIndex = 0;
	uint64_t m_lastFrameUpdateCount = 0;

	size_t m_dynamicLightUpdatePos = 0;

	std::vector<ShadowMap> m_shadowMaps;

	std::vector<LightEntry> m_lights;

	std::array<glm::vec3, 4> m_frustumPlanes[6];

	eg::Pipeline m_depthCopyPipeline;
};
