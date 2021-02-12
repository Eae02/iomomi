#pragma once

#include <EGame/EG.hpp>

#include "QualityLevel.hpp"

class WaterRenderer
{
public:
	WaterRenderer();
	
	void RenderEarly(eg::BufferRef positionsBuffer, uint32_t numParticles, class RenderTexManager& rtManager);
	void RenderPost(class RenderTexManager& rtManager);
	
	static eg::TextureRef GetDummyDepthTexture();
	
	static constexpr QualityLevel HighPrecisionMinQL = QualityLevel::Medium;
	
private:
	void CreateDepthBlurPipelines(uint32_t samples);
	
	QualityLevel m_currentQualityLevel;
	
	eg::Buffer m_quadVB;
	
	eg::Pipeline m_pipelineDepthMin;
	eg::Pipeline m_pipelineDepthMax;
	eg::Pipeline m_pipelineBlurPass1;
	eg::Pipeline m_pipelineBlurPass2;
	eg::Pipeline m_pipelinePostStdQual;
	eg::Pipeline m_pipelinePostHighQual;
	eg::PipelineRef m_pipelinesRefPost[5];
	
	eg::Texture* m_normalMapTexture;
};
