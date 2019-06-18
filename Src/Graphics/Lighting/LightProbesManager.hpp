#pragma once

#include "../DeferredRenderer.hpp"

struct LightProbe
{
	glm::vec3 position;
	glm::vec3 parallaxRad;
	glm::vec3 influenceRad;
	glm::vec3 influenceFade;
};

extern eg::BRDFIntegrationMap* brdfIntegrationMap;

class LightProbesManager
{
public:
	LightProbesManager();
	
	struct RenderArgs
	{
		DeferredRenderer::RenderTarget* renderTarget;
		glm::vec3 cameraPosition;
		glm::mat4 viewProj;
		glm::mat4 invViewProj;
	};
	
	void PrepareWorld(const std::vector<LightProbe>& lightProbes,
		const std::function<void(const RenderArgs&)>& renderCallback);
	
	void PrepareForDraw(const glm::vec3& cameraPosition);
	
	void Bind() const;
	
private:
	static constexpr uint32_t MAX_VISIBLE = 8;
	static constexpr uint32_t RENDER_RESOLUTION = 512;
	static constexpr uint32_t IRRADIANCE_MAP_RESOLUTION = 512;
	static constexpr uint32_t SPF_MAP_RESOLUTION = 512;
	static constexpr uint32_t SPF_MAP_MIP_LEVELS = 8;
	
	eg::Pipeline m_ambientPipeline;
	
	eg::IrradianceMapGenerator m_irradianceMapGenerator;
	eg::SPFMapGenerator m_spfMapGenerator;
	
	eg::Buffer m_probesUniformBuffer;
	eg::DescriptorSet m_descriptorSet;
	
	eg::Texture m_environmentMap;
	std::vector<DeferredRenderer::RenderTarget> m_envMapRenderTargets;
	
	uint32_t m_maxProbes = 0;
	eg::Texture m_irradianceMaps;
	eg::Texture m_spfMaps;
	
	std::vector<LightProbe> m_probes;
};
