#pragma once

#include <EGame/EG.hpp>

class WaterRenderer
{
public:
	class RenderTarget
	{
	public:
		friend class WaterRenderer;
		
		RenderTarget(uint32_t width, uint32_t height, eg::TextureRef inputColor, eg::TextureRef inputDepth,
			eg::TextureRef outputTexture, uint32_t outputArrayLayer);
		
		RenderTarget() = default;
		
		eg::FramebufferRef OutputFramebuffer() const
		{
			return m_outputFramebuffer;
		}
		
	private:
		uint32_t m_width;
		uint32_t m_height;
		
		eg::TextureRef m_inputColor;
		eg::TextureRef m_inputDepth;
		
		eg::Texture m_depthTexture;
		eg::Framebuffer m_depthPassFramebuffer;
		
		eg::Texture m_travelDepthTexture;
		eg::Framebuffer m_travelDepthPassFramebuffer;
		
		eg::Texture m_blurredDepthTexture1;
		eg::Framebuffer m_blurredDepthFramebuffer1;
		eg::Texture m_blurredDepthTexture2;
		eg::Framebuffer m_blurredDepthFramebuffer2;
		
		eg::TextureRef m_outputTexture;
		eg::Framebuffer m_outputFramebuffer;
	};
	
	struct WaterData
	{
		eg::BufferRef positionsBuffer;
		uint32_t numParticles;
	};
	
	WaterRenderer();
	
	void RenderBasic(eg::BufferRef positionsBuffer, uint32_t numParticles) const;
	
	void Render(eg::BufferRef positionsBuffer, uint32_t numParticles, RenderTarget& renderTarget);
	
private:
	eg::Buffer m_quadVB;
	
	eg::Pipeline m_pipelineBasic;
	
	eg::Pipeline m_pipelineDepthOnly;
	eg::Pipeline m_pipelineAdditive;
	eg::Pipeline m_pipelineBlurPass1;
	eg::Pipeline m_pipelineBlurPass2;
	eg::Pipeline m_pipelineBlurSinglePass;
	eg::Pipeline m_pipelinePost;
};
