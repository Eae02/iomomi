#pragma once

#include "ObjectMaterial.hpp"

class GravityCornerMaterial : public ObjectMaterial
{
public:
	GravityCornerMaterial() = default;
	
	eg::PipelineRef GetPipeline() const override;
	void Bind() const override;
	
	static GravityCornerMaterial instance;
};
