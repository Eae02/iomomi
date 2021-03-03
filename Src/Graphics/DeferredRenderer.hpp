#pragma once

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
	
	void DrawPointLights(const std::vector<std::shared_ptr<PointLight>>& pointLights,
		eg::TextureRef waterDepthTexture, RenderTexManager& rtManager, uint32_t shadowResolution) const;
	
	void End() const;
	
	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;
	
private:
	void CreatePipelines();
	
	void PrepareSSAO(RenderTexManager& rtManager) const;
	
	eg::Sampler m_shadowMapSampler;
	
	eg::Pipeline m_ambientPipelineWithSSAO;
	eg::Pipeline m_ambientPipelineWithoutSSAO;
	eg::Pipeline m_spotLightPipeline;
	eg::Pipeline m_pointLightPipelineSoftShadows;
	eg::Pipeline m_pointLightPipelineHardShadows;
	
	eg::Pipeline m_ssaoPipelineLQ;
	eg::Pipeline m_ssaoPipelineHQ;
	eg::Pipeline m_ssaoBlurPipeline;
	eg::Buffer m_ssaoSamplesBuffer;
	eg::Texture m_ssaoRotationsTexture;
};
