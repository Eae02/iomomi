#pragma once

class Renderer
{
public:
	Renderer();
	
	void Begin(const eg::ColorLin* clearColor);
	
	void End();
	
	static constexpr eg::Format DEPTH_FORMAT = eg::Format::Depth16;
	static constexpr eg::Format COLOR_FORMAT = eg::Format::R16G16B16A16_Float;
	
private:
	int m_resX = -1;
	int m_resY = -1;
	
	eg::Texture m_colorTexture;
	eg::Texture m_depthTexture;
	eg::Framebuffer m_mainFramebuffer;
	eg::Sampler m_attachmentSampler;
	
	eg::Pipeline m_postPipeline;
};
