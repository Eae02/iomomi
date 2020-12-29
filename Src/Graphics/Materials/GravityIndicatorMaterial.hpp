#pragma once

class GravityIndicatorMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat3x4 transform;
		glm::vec4 downAndGlowIntensity;
		
		InstanceData(const glm::mat4& _transform, const glm::vec3& down, float glowIntensity)
			: transform(glm::transpose(_transform)), downAndGlowIntensity(down, glowIntensity) { }
	};
	
	GravityIndicatorMaterial();
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	OrderRequirement GetOrderRequirement() const override;
	
	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;
	
private:
	
};
