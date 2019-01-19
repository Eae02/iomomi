#pragma once

class ObjectMaterial
{
public:
	virtual eg::PipelineRef GetPipeline() const = 0;
	virtual void Bind() const = 0;
};
