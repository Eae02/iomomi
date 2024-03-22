#pragma once

class LightStripMaterial : public eg::IMaterial
{
public:
	struct InstanceData
	{
		glm::mat4 transform;
		float lightProgress;
	};

	LightStripMaterial();

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool CheckInstanceDataType(const std::type_info* instanceDataType) const override;

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	struct Parameters
	{
		eg::ColorLin color1;
		eg::ColorLin color2;
		float transitionProgress;

		bool operator==(const Parameters&) const = default;
		bool operator!=(const Parameters&) const = default;
	};

	void SetParameters(const Parameters& parameters);

	static constexpr uint32_t INSTANCE_DATA_BINDING = 2;

private:
	eg::Buffer m_parametersBuffer;
	eg::DescriptorSet m_parametersDescriptorSet;
	std::optional<Parameters> m_currentParameters;
};
