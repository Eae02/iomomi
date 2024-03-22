#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>

#include "BlurRenderer.hpp"

class PostProcessor
{
public:
	PostProcessor() = default;

	void SetRenderTargets(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget);

	void Render(eg::FramebufferHandle output, eg::Format outputFormat, float colorScale);

private:
	struct PipelineVariantKey
	{
		bool enableFXAA;
		eg::Format outputFormat;

		bool operator==(const PipelineVariantKey& other) const = default;
	};

	static eg::Pipeline CreatePipeline(const PipelineVariantKey& variantKey);

	std::vector<std::pair<PipelineVariantKey, eg::Pipeline>> m_pipelines;

	eg::DescriptorSet m_descriptorSet;
};
