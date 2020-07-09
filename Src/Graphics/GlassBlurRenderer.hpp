#pragma once

class GlassBlurRenderer
{
public:
	GlassBlurRenderer();
	
	void MaybeUpdateResolution(uint32_t newWidth, uint32_t newHeight);
	
	void Render(eg::TextureRef inputTexture) const;
	
	eg::TextureRef OutputTexture() const
	{
		return m_blurTextureOut;
	}
	
	static constexpr int LEVELS = 4;
	
private:
	void DoBlurPass(const glm::vec2& blurVector, eg::TextureRef inputTexture,
		int inputLod, eg::FramebufferRef dstFramebuffer) const;
	
	uint32_t m_inputWidth = 0;
	uint32_t m_inputHeight = 0;
	
	eg::Sampler m_outputTextureSampler;
	
	eg::Pipeline m_blurPipeline;
	eg::Texture m_blurTextureTmp;
	eg::Texture m_blurTextureOut;
	
	eg::Framebuffer m_framebuffersTmp[LEVELS];
	eg::Framebuffer m_framebuffersOut[LEVELS];
};
