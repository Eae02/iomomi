#pragma once

#include "StaticPropMaterial.hpp"

class PumpScreenMaterial : public eg::IMaterial
{
public:
	using InstanceData = StaticPropMaterial::InstanceData;
	
	PumpScreenMaterial();
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	void Update(float dt, int direction);
	
private:
	eg::DescriptorSet m_descriptorSet;
	eg::Buffer m_dataBuffer;
	float m_animationProgress = 0;
	float m_opacity = 0;
	int m_currentDirection = 0;
};
