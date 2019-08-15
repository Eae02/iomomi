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
		RenderTarget(uint32_t width, uint32_t height, uint32_t samples,
			eg::TextureRef outputTexture, uint32_t outputArrayLayer);
		
		uint32_t Width() const
		{
			return m_width;
		}
		
		uint32_t Height() const
		{
			return m_height;
		}
		
		uint32_t Samples() const
		{
			return m_samples;
		}
		
		eg::TextureRef ResolvedDepthTexture() const
		{
			return m_samples <= 1 ? m_gbDepthTexture : m_resolvedDepthTexture;
		}
		
	private:
		friend class DeferredRenderer;
		
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_samples;
		
		eg::Texture m_gbColor1Texture;
		eg::Texture m_gbColor2Texture;
		eg::Texture m_gbDepthTexture;
		eg::Framebuffer m_gbFramebuffer;
		
		eg::Texture m_resolvedDepthTexture;
		
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
	
	void BeginEmissive(RenderTarget& target, bool hasWater);
	
	void BeginLighting(RenderTarget& target, bool hasWater) const;
	
	void DrawReflectionPlaneLighting(RenderTarget& target, const std::vector<struct ReflectionPlane*>& planes);
	
	void DrawSpotLights(RenderTarget& target, const std::vector<SpotLightDrawData>& spotLights) const;
	void DrawPointLights(RenderTarget& target, const std::vector<PointLightDrawData>& pointLights) const;
	
	void End(RenderTarget& target) const;
	
	void DrawWaterBasic(eg::BufferRef positionsBuffer, uint32_t numParticles) const;
	
	void DrawWater(RenderTarget& target, eg::BufferRef positionsBuffer, uint32_t numParticles);
	
	void PollSettingsChanged();
	
	static eg::StencilState MakeStencilState(uint32_t reference)
	{
		return { eg::StencilOp::Keep, eg::StencilOp::Replace, eg::StencilOp::Keep, eg::CompareOp::Always, 0, 0xFF, reference };
	}
	
	static constexpr eg::Format DEPTH_FORMAT = eg::Format::Depth24Stencil8;
	static constexpr eg::Format LIGHT_COLOR_FORMAT_LDR = eg::Format::R8G8B8A8_UNorm;
	static constexpr eg::Format LIGHT_COLOR_FORMAT_HDR = eg::Format::R16G16B16A16_Float;
	
	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;
	
private:
	void CreatePipelines();
	
	eg::Sampler m_shadowMapSampler;
	
	uint32_t m_currentSampleCount = 0;
	
	eg::Buffer m_reflectionPlaneVertexBuffer;
	
	eg::Pipeline m_ambientPipeline;
	eg::Pipeline m_reflectionPlanePipeline;
	eg::Pipeline m_spotLightPipeline;
	eg::Pipeline m_pointLightPipelineSoftShadows;
	eg::Pipeline m_pointLightPipelineHardShadows;
	
	eg::BRDFIntegrationMap m_brdfIntegMap;
	WaterRenderer m_waterRenderer;
};
