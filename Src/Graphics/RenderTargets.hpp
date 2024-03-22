#pragma once

struct RenderTargetsCreateArgs
{
	uint32_t resX;
	uint32_t resY;
	bool enableWater;
	bool enableSSR;
	bool enableBlurredGlass;
};

struct RenderStageOutputs
{
	eg::Framebuffer framebuffer;
	eg::Texture outputTexture;
};

struct RenderTargets
{
	uint32_t uid = 0;
	uint32_t resX = 0;
	uint32_t resY = 0;

	eg::Texture gbufferColor1;
	eg::Texture gbufferColor2;
	eg::Texture gbufferDepth;

	std::optional<eg::Texture> waterDepthTexture;
	std::optional<eg::TextureRef> glassBlurTexture;

	std::optional<eg::DescriptorSet> waterDepthTextureDescriptorSet;
	std::optional<eg::DescriptorSet> glassBlurTextureDescriptorSet;
	std::optional<eg::DescriptorSet> blurredGlassDepthTextureDescriptorSet;

	std::optional<RenderStageOutputs> blurredGlassDepthStage;
	RenderStageOutputs lightingStage;

	eg::TextureRef waterPostInput;
	std::optional<RenderStageOutputs> waterPostStage;

	eg::TextureRef ssrColorInput;
	std::optional<RenderStageOutputs> ssrStage;
	std::optional<eg::DescriptorSet> ssrAdditionalDescriptorSet;

	eg::TextureRef finalLitTexture;
	eg::FramebufferRef finalLitTextureFramebuffer;
	eg::DescriptorSet finalLitTextureDescriptorSet;

	bool IsValid(const RenderTargetsCreateArgs& args) const;

	eg::TextureRef GetWaterDepthTextureOrDummy() const;
	eg::DescriptorSetRef GetWaterDepthTextureDescriptorSetOrDummy() const;

	static RenderTargets Create(const RenderTargetsCreateArgs& args);

	eg::Texture MakeAttachment(
		eg::Format format, const char* label, eg::TextureFlags extraFlags = eg::TextureFlags::ShaderSample
	) const;
};

constexpr eg::Format GB_DEPTH_FORMAT = eg::Format::Depth16;
constexpr eg::Format GB_COLOR_FORMAT = eg::Format::R8G8B8A8_UNorm;

extern eg::Format lightColorAttachmentFormat;

void SetLightColorAttachmentFormat(bool enableHDR);
