#pragma once

class SSAO
{
public:
	SSAO();

	void RenderTargetsCreated(const struct RenderTargets& renderTargets);

	void RunSSAOPass();

	eg::TextureRef OutputTexture() const { return m_texBlurred; }

private:
	eg::Buffer m_parametersBuffer;
	uint32_t m_ssaoSamplesBufferCurrentSamples = 0;
	eg::Texture m_ssaoRotationsTexture;

	uint32_t m_blurParametersBufferSecondOffset;
	eg::Buffer m_blurParametersBuffer;

	eg::Texture m_texLinearDepth;
	eg::Framebuffer m_fbLinearDepth;

	eg::Texture m_texUnblurred;
	eg::Framebuffer m_fbUnblurred;

	eg::Texture m_texBlurredOneAxis;
	eg::Framebuffer m_fbBlurredOneAxis;

	eg::Texture m_texBlurred;
	eg::Framebuffer m_fbBlurred;

	eg::DescriptorSet m_depthLinearizeDS;
	eg::DescriptorSet m_mainPassDS0;
	eg::DescriptorSet m_mainPassAttachmentsDS;
	eg::DescriptorSet m_blurPass1DS;
	eg::DescriptorSet m_blurPass2DS;
};
