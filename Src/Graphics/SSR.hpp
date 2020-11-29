#pragma once

#include "QualityLevel.hpp"
#include "RenderTex.hpp"

class SSR
{
public:
	SSR() = default;
	
	void Render(eg::TextureRef waterDepth, RenderTex destinationTexture, RenderTexManager& rtManager);
	
private:
	void CreatePipeline();
	
	QualityLevel m_currentReflectionQualityLevel = (QualityLevel)-1;
	
	eg::Pipeline m_pipeline;
};
