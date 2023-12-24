#pragma once

#include "StaticPropMaterial.hpp"

enum class PumpDirection
{
	None,
	Left,
	Right
};

class PumpScreenMaterial : public eg::IMaterial
{
public:
	using InstanceData = StaticPropMaterial::InstanceData;

	PumpScreenMaterial();

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	void Update(float dt, PumpDirection direction);

	static const eg::ColorLin backgroundColor;
	static const eg::ColorLin color;

private:
	eg::DescriptorSet m_descriptorSet;
	eg::Buffer m_dataBuffer;
	float m_animationProgress = 0;
	float m_opacity = 0;
	PumpDirection m_currentDirection = PumpDirection::None;
};
