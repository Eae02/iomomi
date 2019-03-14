#pragma once

#include "ObjectMaterial.hpp"

class GravityCornerMaterial : public ObjectMaterial
{
public:
	GravityCornerMaterial() = default;
	
	eg::PipelineRef GetPipeline(PipelineType pipelineType, bool flipWinding) const override;
	void Bind(ObjectMaterial::PipelineType boundPipeline) const override;
	
	static GravityCornerMaterial instance;
};
