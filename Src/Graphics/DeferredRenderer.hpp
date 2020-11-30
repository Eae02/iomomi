#pragma once

#include "Lighting/SpotLight.hpp"
#include "Lighting/PointLight.hpp"
#include "WaterRenderer.hpp"
#include "RenderTex.hpp"

class DeferredRenderer
{
public:
	DeferredRenderer();
	
	void BeginGeometry(RenderTexManager& rtManager) const;
	void BeginGeometryFlags(RenderTexManager& rtManager) const;
	void EndGeometry(RenderTexManager& rtManager) const;
	
	void BeginTransparent(RenderTex destinationTexture, RenderTexManager& rtManager);
	
	void BeginLighting(RenderTexManager& rtManager);
	void EndTransparent();
	
	void DrawSpotLights(const std::vector<SpotLightDrawData>& spotLights, RenderTexManager& rtManager) const;
	void DrawPointLights(const std::vector<PointLightDrawData>& pointLights, eg::TextureRef waterDepthTexture, RenderTexManager& rtManager) const;
	
	void End() const;
	
	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;
	
private:
	void CreatePipelines();
	
	eg::Sampler m_shadowMapSampler;
	
	eg::Pipeline m_ambientPipeline;
	eg::Pipeline m_spotLightPipeline;
	eg::Pipeline m_pointLightPipelineSoftShadows;
	eg::Pipeline m_pointLightPipelineHardShadows;
	
	eg::BRDFIntegrationMap m_brdfIntegMap;
};
