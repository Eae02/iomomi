#pragma once

class PostProcessor
{
public:
	PostProcessor();
	
	void Render(eg::TextureRef input, eg::TextureRef bloomTexture);
	
private:
	eg::Pipeline m_postPipeline;
	eg::Sampler m_inputSampler;
};
