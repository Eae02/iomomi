#pragma once

#include "QualityLevel.hpp"
#include "RenderTex.hpp"

class SSR
{
public:
	SSR() = default;
	
	void Render(eg::TextureRef waterDepth, RenderTex destinationTexture, RenderTexManager& rtManager, const eg::ColorSRGB& fallbackColor);
	
private:
	void CreatePipeline();
	
	QualityLevel m_currentReflectionQualityLevel = (QualityLevel)-1;
	
	eg::Pipeline m_pipelineInitial;
	eg::Pipeline m_blur1Pipeline;
	eg::Pipeline m_blur2Pipeline;
};
