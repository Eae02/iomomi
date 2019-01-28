#pragma once

#include "ObjectMaterial.hpp"

class GravityCornerMaterial : public ObjectMaterial
{
public:
	GravityCornerMaterial() = default;
	
	eg::PipelineRef GetPipeline(PipelineType pipelineType) const override;
	void Bind() const override;
	
	static GravityCornerMaterial instance;
};
