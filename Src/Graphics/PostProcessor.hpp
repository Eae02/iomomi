#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>
#include "BlurRenderer.hpp"

class PostProcessor
{
public:
	PostProcessor() = default;
	
	void Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget,
		eg::FramebufferHandle output, uint32_t outputResX, uint32_t outputResY, float colorScale);
	
private:
	void InitPipeline();
	
	std::optional<bool> m_bloomWasEnabled;
	std::optional<bool> m_fxaaWasEnabled;
	
	eg::Pipeline m_pipeline;
};
