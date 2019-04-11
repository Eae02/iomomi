#pragma once

struct LightStripMaterial : public eg::IMaterial
{
	struct InstanceData
	{
		glm::mat4 transform;
		float lightProgress;
	};
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	eg::ColorLin color1;
	eg::ColorLin color2;
	float transitionProgress;
};
