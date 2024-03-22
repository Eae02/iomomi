#pragma once

struct WaterRenderPipelines
{
	eg::Pipeline pipelineDepthMin;
	eg::Pipeline pipelineDepthMax;
	eg::Pipeline pipelineBlurPass1;
	eg::Pipeline pipelineBlurPass2;
	eg::Pipeline pipelinePostStdQual;
	eg::Pipeline pipelinePostHighQual;

	uint32_t depthBlurSamples;

	eg::DescriptorSet waterPostDescriptorSet0;

	static WaterRenderPipelines instance;

	void Init();

	void CreateDepthBlurPipelines(uint32_t samples);
};

class WaterRenderer
{
public:
	WaterRenderer();

	void SetPositionsBuffer(eg::BufferRef positionsBuffer, uint32_t numParticles);

	void RenderTargetsCreated(const struct RenderTargets& renderTargets);

	void RenderEarly(const struct RenderTargets& renderTargets);
	void RenderPost(const struct RenderTargets& renderTargets);

	static eg::TextureRef GetDummyDepthTexture();
	static eg::DescriptorSetRef GetDummyDepthTextureDescriptorSet();

	static constexpr eg::Format GLOW_INTENSITY_TEXTURE_FORMAT = eg::Format::R8_UNorm;
	static constexpr eg::Format BLURRED_DEPTH_TEXTURE_FORMAT = eg::Format::R16G16_Float;
	static constexpr eg::Format DEPTH_TEXTURE_FORMAT = eg::Format::Depth16;

private:
	eg::Texture texGlowIntensity;
	eg::Texture texMinDepth;
	eg::Texture texMaxDepth;
	eg::Texture texDepthBlurred1;

	eg::Framebuffer depthMinFramebuffer;
	eg::Framebuffer depthMaxFramebuffer;
	eg::Framebuffer blur1Framebuffer;
	eg::Framebuffer blur2Framebuffer;

	eg::DescriptorSet m_waterPositionsDescriptorSet;
	eg::DescriptorSet m_gbufferDepthDescriptorSet;

	eg::DescriptorSet m_blur1DescriptorSet;
	eg::DescriptorSet m_blur2DescriptorSet;

	eg::DescriptorSet m_waterPostDescriptorSet1;

	uint32_t m_numParticles = 0;
};
