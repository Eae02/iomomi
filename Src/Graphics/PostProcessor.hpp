#pragma once

class PostProcessor
{
public:
	PostProcessor();
	
	void Render(eg::TextureRef input);
	
private:
	eg::Pipeline m_postPipeline;
	eg::Sampler m_inputSampler;
};
