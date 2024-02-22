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

	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	eg::ColorLin color1;
	eg::ColorLin color2;
	float transitionProgress;

	static constexpr uint32_t INSTANCE_DATA_BINDING = 2;
};
