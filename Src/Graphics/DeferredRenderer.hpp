#pragma once

#include "Lighting/SpotLight.hpp"
#include "Lighting/PointLight.hpp"
#include "WaterRenderer.hpp"

class DeferredRenderer
{
public:
	class RenderTarget
	{
	public:
		RenderTarget(uint32_t width, uint32_t height,
			eg::TextureRef outputTexture, uint32_t outputArrayLayer, QualityLevel waterQuality);
		
		uint32_t Width() const
		{
			return m_width;
		}
		
		uint32_t Height() const
		{
			return m_height;
		}
		
		eg::TextureRef DepthTexture() const
		{
			return m_gbDepthTexture;
		}
		
	private:
		friend class DeferredRenderer;
		
		uint32_t m_width;
		uint32_t m_height;
		
		eg::Texture m_gbColor1Texture;
		eg::Texture m_gbColor2Texture;
		eg::Texture m_gbDepthTexture;
		eg::Framebuffer m_gbFramebuffer;
		
		eg::Texture m_flagsTexture;
		eg::Framebuffer m_flagsFramebuffer;
		
		eg::Texture m_emissiveTexture;
		
		eg::Framebuffer m_lightingFramebuffer;
		eg::Framebuffer m_emissiveFramebuffer;
		
		eg::Framebuffer m_lightingFramebufferNoWater;
		eg::Framebuffer m_emissiveFramebufferNoWater;
		
		eg::Texture m_lightingOutputTexture;
		WaterRenderer::RenderTarget m_waterRT;
		
		eg::TextureRef m_waterOutputTexture;
	};
	
	DeferredRenderer();
	
	void BeginGeometry(RenderTarget& target) const;
	
	void BeginGeometryFlags(RenderTarget& target) const;
	
	void BeginEmissive(RenderTarget& target, bool hasWater);
	
	void BeginLighting(RenderTarget& target, bool hasWater) const;
	
	void DrawSpotLights(RenderTarget& target, const std::vector<SpotLightDrawData>& spotLights) const;
	void DrawPointLights(RenderTarget& target, const std::vector<PointLightDrawData>& pointLights) const;
	
	void End(RenderTarget& target) const;
	
	void DrawWaterBasic(eg::BufferRef positionsBuffer, uint32_t numParticles) const;
	
	void DrawWater(RenderTarget& target, eg::BufferRef positionsBuffer, uint32_t numParticles);
	
	void PollSettingsChanged();
	
	static constexpr eg::Format DEPTH_FORMAT = eg::Format::Depth32;
	static constexpr eg::Format FLAGS_FORMAT = eg::Format::R8_UInt;
	static constexpr eg::Format LIGHT_COLOR_FORMAT_LDR = eg::Format::R8G8B8A8_UNorm;
	static constexpr eg::Format LIGHT_COLOR_FORMAT_HDR = eg::Format::R16G16B16A16_Float;
	
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
	WaterRenderer m_waterRenderer;
};
