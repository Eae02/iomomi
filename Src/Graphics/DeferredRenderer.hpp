#pragma once

#include "Lighting/SpotLight.hpp"
#include "Lighting/PointLight.hpp"

class DeferredRenderer
{
public:
	class RenderTarget
	{
	public:
		RenderTarget(uint32_t width, uint32_t height, eg::TextureRef outputTexture, uint32_t outputArrayLayer);
		
		uint32_t Width() const
		{
			return m_width;
		}
		
		uint32_t Height() const
		{
			return m_height;
		}
		
	private:
		friend class DeferredRenderer;
		
		uint32_t m_width;
		uint32_t m_height;
		
		eg::Texture m_gbColor1Texture;
		eg::Texture m_gbColor2Texture;
		eg::Texture m_gbDepthTexture;
		eg::Framebuffer m_gbFramebuffer;
		
		eg::TextureRef m_outputTexture;
		eg::Framebuffer m_outputFramebuffer;
	};
	
	DeferredRenderer();
	
	void BeginGeometry(RenderTarget& target) const;
	
	void BeginLighting(RenderTarget& target, const class LightProbesManager* lightProbesManager) const;
	
	void DrawSpotLights(RenderTarget& target, const std::vector<SpotLightDrawData>& spotLights) const;
	void DrawPointLights(RenderTarget& target, const std::vector<PointLightDrawData>& pointLights) const;
	
	void End(RenderTarget& target) const;
	
	static constexpr eg::Format DEPTH_FORMAT = eg::Format::Depth32;
	static constexpr eg::Format LIGHT_COLOR_FORMAT = eg::Format::R16G16B16A16_Float;
	
	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;
	
private:
	eg::Sampler m_shadowMapSampler;
	
	eg::Pipeline m_constantAmbientPipeline;
	eg::Pipeline m_spotLightPipeline;
	eg::Pipeline m_pointLightPipeline;
};
