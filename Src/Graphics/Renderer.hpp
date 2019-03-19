#pragma once

#include "Lighting/SpotLight.hpp"
#include "Lighting/PointLight.hpp"

class Renderer
{
public:
	Renderer();
	
	void BeginGeometry();
	
	void BeginLighting();
	
	void DrawSpotLights(const std::vector<SpotLightDrawData>& spotLights);
	void DrawPointLights(const std::vector<PointLightDrawData>& pointLights);
	
	void End();
	
	static constexpr eg::Format DEPTH_FORMAT = eg::Format::Depth16;
	static constexpr eg::Format LIGHT_COLOR_FORMAT = eg::Format::R16G16B16A16_Float;
	
	static const eg::FramebufferFormatHint GEOMETRY_FB_FORMAT;
	
private:
	int m_resX = -1;
	int m_resY = -1;
	
	eg::Texture m_gbColor1Texture;
	eg::Texture m_gbColor2Texture;
	eg::Texture m_gbDepthTexture;
	eg::Framebuffer m_gbFramebuffer;
	
	eg::Texture m_lightOutTexture;
	eg::Framebuffer m_lightOutFramebuffer;
	
	eg::Sampler m_attachmentSampler;
	eg::Sampler m_shadowMapSampler;
	
	eg::Pipeline m_postPipeline;
	eg::Pipeline m_ambientPipeline;
	eg::Pipeline m_spotLightPipeline;
	eg::Pipeline m_pointLightPipeline;
};
