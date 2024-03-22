#pragma once

class BlurRenderer
{
public:
	BlurRenderer(uint32_t blurLevels, eg::Format format);

	void MaybeUpdateResolution(uint32_t newWidth, uint32_t newHeight);

	void Render(eg::DescriptorSetRef inputTextureDescriptorSet, float blurScale) const;

	const eg::Texture& OutputTexture() const { return m_blurTextureOut; }

	eg::DescriptorSetRef OutputDescriptorSet() const { return m_blurXDescriptorSets.back(); }

	uint32_t NumLevels() const { return m_levels; }

private:
	void DoBlurPass(
		const glm::vec2& blurVector, const glm::vec2& sampleOffset, eg::DescriptorSetRef inputTextureDescriptorSet,
		int inputLod, eg::FramebufferRef dstFramebuffer
	) const;

	uint32_t m_levels;
	eg::Format m_format;

	std::vector<eg::DescriptorSet> m_blurXDescriptorSets;
	std::vector<eg::DescriptorSet> m_blurYDescriptorSets;

	eg::PipelineRef m_blurPipeline;

	uint32_t m_inputWidth = 0;
	uint32_t m_inputHeight = 0;

	eg::Texture m_blurTextureTmp;
	eg::Texture m_blurTextureOut;

	std::vector<eg::Framebuffer> m_framebuffersTmp;
	std::vector<eg::Framebuffer> m_framebuffersOut;
};
