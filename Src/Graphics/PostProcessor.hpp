#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>

#include "BlurRenderer.hpp"

class PostProcessor
{
public:
	PostProcessor() = default;

	void Render(
		eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget, eg::FramebufferHandle output,
		eg::Format outputFormat, uint32_t outputResX, uint32_t outputResY, float colorScale
	);

private:
	struct PipelineVariantKey
	{
		bool enableFXAA;
		eg::Format outputFormat;

		bool operator==(const PipelineVariantKey& other) const = default;
	};

	static eg::Pipeline CreatePipeline(const PipelineVariantKey& variantKey);

	std::vector<std::pair<PipelineVariantKey, eg::Pipeline>> m_pipelines;
};
