#pragma once

#include "Lighting/SpotLight.hpp"
#include "Lighting/PointLight.hpp"
#include "WaterRenderer.hpp"

class DeferredRenderer
{
public:
	DeferredRenderer();
	
	void BeginGeometry() const;
	void BeginGeometryFlags() const;
	void EndGeometry() const;
	
	void BeginTransparent(bool isBeforeWater);
	
	void BeginLighting(bool hasWater);
	void EndTransparent(bool isBeforeWater);
	
	void DrawSpotLights(const std::vector<SpotLightDrawData>& spotLights) const;
	void DrawPointLights(const std::vector<PointLightDrawData>& pointLights) const;
	
	void End() const;
	
	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;
	
private:
	void CreatePipelines();
	
	eg::Sampler m_shadowMapSampler;
	
	QualityLevel m_currentReflectionQualityLevel = (QualityLevel)-1;
	
	eg::Pipeline m_ambientPipeline;
	eg::Pipeline m_spotLightPipeline;
	eg::Pipeline m_pointLightPipelineSoftShadows;
	eg::Pipeline m_pointLightPipelineHardShadows;
	
	eg::BRDFIntegrationMap m_brdfIntegMap;
};
