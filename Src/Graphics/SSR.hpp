#pragma once

#include "QualityLevel.hpp"
#include "GraphicsCommon.hpp"

class SSR
{
public:
	SSR() = default;
	
	void Render(eg::TextureRef waterDepth, RenderTex destinationTexture);
	
private:
	void CreatePipeline();
	
	QualityLevel m_currentReflectionQualityLevel = (QualityLevel)-1;
	
	eg::Pipeline m_pipeline;
};
