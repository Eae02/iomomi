#include "RenderTargets.hpp"
#include "../Water/WaterRenderer.hpp"
#include "GraphicsCommon.hpp"
#include "SSR.hpp"

eg::Texture RenderTargets::MakeAttachment(eg::Format format, const char* label, eg::TextureFlags extraFlags) const
{
	return eg::Texture::Create2D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::FramebufferAttachment | extraFlags,
		.mipLevels = 1,
		.width = resX,
		.height = resY,
		.format = format,
		.label = label,
	});
}

static uint32_t nextUID = 1;

static const eg::DescriptorSetBinding SSR_ADDITIONAL_SET_BINDINGS[] = {
	// gbuffer color 2
	eg::DescriptorSetBinding{
		.binding = 0,
		.type = eg::BindingTypeTexture{ .viewType = eg::TextureViewType::Flat2D },
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	// gbuffer depth
	eg::DescriptorSetBinding{
		.binding = 1,
		.type =
			eg::BindingTypeTexture{
				.viewType = eg::TextureViewType::Flat2D,
				.sampleMode = eg::TextureSampleMode::UnfilterableFloat,
			},
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
	// nearest clamp sampler
	eg::DescriptorSetBinding{
		.binding = 2,
		.type = eg::BindingTypeSampler::Nearest,
		.shaderAccess = eg::ShaderAccessFlags::Fragment,
	},
};

RenderTargets RenderTargets::Create(const RenderTargetsCreateArgs& args)
{
	RenderTargets renderTargets;
	renderTargets.uid = nextUID++;
	renderTargets.resX = args.resX;
	renderTargets.resY = args.resY;

	renderTargets.gbufferColor1 = renderTargets.MakeAttachment(GB_COLOR_FORMAT, "GBColor1");
	renderTargets.gbufferColor2 = renderTargets.MakeAttachment(GB_COLOR_FORMAT, "GBColor2");
	renderTargets.gbufferDepth = renderTargets.MakeAttachment(GB_DEPTH_FORMAT, "GBDepth");

	// Water post pass
	if (args.enableWater)
	{
		renderTargets.waterDepthTexture =
			renderTargets.MakeAttachment(WaterRenderer::BLURRED_DEPTH_TEXTURE_FORMAT, "WaterBlurredDepth");

		renderTargets.waterDepthTextureDescriptorSet = eg::DescriptorSet({ &FRAGMENT_SHADER_TEXTURE_BINDING, 1 });
		renderTargets.waterDepthTextureDescriptorSet->BindTexture(*renderTargets.waterDepthTexture, 0);
	}

	if (args.enableBlurredGlass)
	{
		eg::Texture outputTexture = renderTargets.MakeAttachment(eg::Format::Depth16, "BlurredGlassDepth");
		renderTargets.blurredGlassDepthStage = RenderStageOutputs{
			.framebuffer = eg::Framebuffer({}, outputTexture.handle),
			.outputTexture = std::move(outputTexture),
		};

		renderTargets.glassBlurTextureDescriptorSet = eg::DescriptorSet({ &FRAGMENT_SHADER_TEXTURE_BINDING, 1 });

		renderTargets.blurredGlassDepthTextureDescriptorSet =
			eg::DescriptorSet({ &FRAGMENT_SHADER_TEXTURE_BINDING_UNFILTERABLE, 1 });

		renderTargets.blurredGlassDepthTextureDescriptorSet->BindTexture(
			renderTargets.blurredGlassDepthStage->outputTexture, 0
		);
	}

	// Deferred lighting pass
	renderTargets.lightingStage.outputTexture = renderTargets.MakeAttachment(lightColorAttachmentFormat, "GBLight");
	eg::FramebufferAttachment lightingColorAttachment(renderTargets.lightingStage.outputTexture.handle);
	renderTargets.lightingStage.framebuffer =
		eg::Framebuffer({ &lightingColorAttachment, 1 }, renderTargets.gbufferDepth.handle);

	renderTargets.finalLitTexture = renderTargets.lightingStage.outputTexture;
	renderTargets.finalLitTextureFramebuffer = renderTargets.lightingStage.framebuffer;

	// Water post pass
	if (args.enableWater)
	{
		eg::Texture outputTexture = renderTargets.MakeAttachment(lightColorAttachmentFormat, "LitWithWater");
		eg::FramebufferAttachment attachment(outputTexture.handle);

		renderTargets.waterPostInput = renderTargets.finalLitTexture;
		renderTargets.finalLitTexture = outputTexture;

		renderTargets.waterPostStage = RenderStageOutputs{
			.framebuffer = eg::Framebuffer({ &attachment, 1 }, renderTargets.gbufferDepth.handle),
			.outputTexture = std::move(outputTexture),
		};

		renderTargets.finalLitTextureFramebuffer = renderTargets.waterPostStage->framebuffer;
	}

	if (args.enableSSR)
	{
		eg::Texture outputTexture = renderTargets.MakeAttachment(SSR::COLOR_FORMAT, "LitWithSSR");
		eg::FramebufferAttachment attachment(outputTexture.handle);

		renderTargets.ssrColorInput = renderTargets.finalLitTexture;
		renderTargets.finalLitTexture = outputTexture;

		renderTargets.ssrStage = RenderStageOutputs{
			.framebuffer = eg::Framebuffer({ &attachment, 1 }, renderTargets.gbufferDepth.handle),
			.outputTexture = std::move(outputTexture),
		};

		renderTargets.finalLitTextureFramebuffer = renderTargets.ssrStage->framebuffer;

		renderTargets.ssrAdditionalDescriptorSet = eg::DescriptorSet(SSR_ADDITIONAL_SET_BINDINGS);
		renderTargets.ssrAdditionalDescriptorSet->BindTexture(renderTargets.gbufferColor2, 0);
		renderTargets.ssrAdditionalDescriptorSet->BindTexture(
			renderTargets.gbufferDepth, 1, {}, eg::TextureUsage::DepthStencilReadOnly
		);
		renderTargets.ssrAdditionalDescriptorSet->BindSampler(samplers::nearestClamp, 2);
	}

	renderTargets.finalLitTextureDescriptorSet = eg::DescriptorSet({ &FRAGMENT_SHADER_TEXTURE_BINDING, 1 });
	renderTargets.finalLitTextureDescriptorSet.BindTexture(renderTargets.finalLitTexture, 0);

	return renderTargets;
}

bool RenderTargets::IsValid(const RenderTargetsCreateArgs& args) const
{
	return args.resX == resX && args.resY == resY && args.enableWater == waterPostStage.has_value() &&
	       args.enableSSR == ssrStage.has_value() && args.enableBlurredGlass == blurredGlassDepthStage.has_value();
}

eg::TextureRef RenderTargets::GetWaterDepthTextureOrDummy() const
{
	if (waterDepthTexture.has_value())
		return *waterDepthTexture;
	return WaterRenderer::GetDummyDepthTexture();
}

eg::DescriptorSetRef RenderTargets::GetWaterDepthTextureDescriptorSetOrDummy() const
{
	if (waterDepthTextureDescriptorSet.has_value())
		return *waterDepthTextureDescriptorSet;
	return WaterRenderer::GetDummyDepthTextureDescriptorSet();
}

constexpr eg::Format LIGHT_COLOR_FORMAT_LDR = eg::Format::R8G8B8A8_UNorm;
constexpr eg::Format LIGHT_COLOR_FORMAT_HDR = eg::Format::R16G16B16A16_Float;

eg::Format lightColorAttachmentFormat;

void SetLightColorAttachmentFormat(bool enableHDR)
{
	lightColorAttachmentFormat = enableHDR ? LIGHT_COLOR_FORMAT_HDR : LIGHT_COLOR_FORMAT_LDR;
}
