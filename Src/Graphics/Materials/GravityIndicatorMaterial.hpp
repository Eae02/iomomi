#pragma once

class GravityIndicatorMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat3x4 transform;
		glm::vec3 down;
		float minIntensity;
		float maxIntensity;

		InstanceData(const glm::mat4& _transform) : transform(glm::transpose(_transform)) {}
	};

	GravityIndicatorMaterial() = default;

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	OrderRequirement GetOrderRequirement() const override;

	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;

	static GravityIndicatorMaterial instance;
};
