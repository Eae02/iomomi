#pragma once

class EmissiveMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat4 transform;
		glm::vec4 color;
	};
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyUnordered; }
	
	static EmissiveMaterial instance;
};
