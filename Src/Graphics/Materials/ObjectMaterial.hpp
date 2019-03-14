#pragma once

class ObjectMaterial
{
public:
	enum class PipelineType
	{
		Game,
		Editor
	};
	
	virtual eg::PipelineRef GetPipeline(PipelineType pipelineType, bool flipWinding) const = 0;
	virtual void Bind(ObjectMaterial::PipelineType boundPipeline) const = 0;
};
