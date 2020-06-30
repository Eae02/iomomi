#pragma once

#include "QualityLevel.hpp"

class SSR
{
public:
	SSR() = default;
	
	void Render(eg::TextureRef waterDepth);
	
private:
	void CreatePipeline();
	
	QualityLevel m_currentReflectionQualityLevel = (QualityLevel)-1;
	
	eg::Pipeline m_pipeline;
};
