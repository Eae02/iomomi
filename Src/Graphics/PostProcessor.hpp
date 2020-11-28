#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>

class PostProcessor
{
public:
	PostProcessor();
	
	void Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget,
		eg::FramebufferHandle output = nullptr);
	
private:
	void InitPipeline();
	
	std::optional<bool> m_bloomWasEnabled;
	std::optional<bool> m_fxaaWasEnabled;
	
	eg::Pipeline m_pipeline;
};
