#pragma once

#include "QualityLevel.hpp"
#include "RenderTex.hpp"

class SSR
{
public:
	SSR() = default;
	
	struct SSRRenderArgs
	{
		eg::TextureRef waterDepth;
		RenderTex destinationTexture;
		RenderTexManager* rtManager;
		eg::ColorLin fallbackColor;
		float intensity;
		std::function<void()> renderAdditionalCallback;
	};
	
	void Render(const SSRRenderArgs& renderArgs);
	
	static const float MAX_DISTANCE;
	
private:
	void CreatePipeline();
	
	QualityLevel m_currentReflectionQualityLevel = (QualityLevel)-1;
	
	eg::Pipeline m_pipelineInitial;
	eg::Pipeline m_pipelineBlendPass;
	eg::Pipeline m_blur1Pipeline;
	eg::Pipeline m_blur2Pipeline;
};
