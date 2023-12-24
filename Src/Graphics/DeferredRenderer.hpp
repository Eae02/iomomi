#pragma once

#include "Lighting/PointLight.hpp"
#include "RenderTex.hpp"
#include "Water/WaterRenderer.hpp"

class DeferredRenderer
{
public:
	DeferredRenderer();

	void BeginGeometry(RenderTexManager& rtManager) const;
	void EndGeometry(RenderTexManager& rtManager) const;

	void BeginTransparent(RenderTex destinationTexture, RenderTexManager& rtManager);

	void BeginLighting(RenderTexManager& rtManager);
	void EndTransparent();

	void DrawPointLights(
		const std::vector<std::shared_ptr<PointLight>>& pointLights, bool hasWater, eg::TextureRef waterDepthTexture,
		RenderTexManager& rtManager, uint32_t shadowResolution) const;

	void End() const;

	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;

private:
	void CreatePipelines();

	void PrepareSSAO(RenderTexManager& rtManager);

	eg::Sampler m_shadowMapSampler;

	enum
	{
		SHADOW_MODE_NONE = 0,
		SHADOW_MODE_HARD = 1,
		SHADOW_MODE_SOFT = 2
	};

	eg::Pipeline m_ambientPipelineWithSSAO;
	eg::Pipeline m_ambientPipelineWithoutSSAO;
	eg::Pipeline m_pointLightPipelines[3][2]; // first index is shadow mode, second index is water mode

	eg::Pipeline m_ssaoDepthLinPipeline;
	eg::Pipeline m_ssaoPipelines[3];
	eg::Pipeline m_ssaoBlurPipeline;
	eg::Buffer m_ssaoSamplesBuffer;
	uint32_t m_ssaoSamplesBufferCurrentSamples = 0;
	eg::Texture m_ssaoRotationsTexture;
};
