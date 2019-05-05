#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>

class PostProcessor
{
public:
	PostProcessor();
	
	void Render(eg::TextureRef input, const eg::BloomRenderer::RenderTarget* bloomRenderTarget);
	
	float exposure = 1.2f;
	float bloomIntensity = 0.5f;
	
private:
	eg::Pipeline m_pipelineNoBloom;
	eg::Pipeline m_pipelineBloom;
	eg::Sampler m_inputSampler;
};
