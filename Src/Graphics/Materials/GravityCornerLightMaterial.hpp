#pragma once

class GravityCornerLightMaterial : public eg::IMaterial
{
public:
	GravityCornerLightMaterial() = default;

	void Initialize();

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyUnordered; }

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	void Activate(const glm::vec3& position);

	void Update(float dt);

	static GravityCornerLightMaterial instance;

private:
	glm::vec3 m_activationPos;
	float m_activationIntensity = 0;

	eg::Buffer m_parametersBuffer;
	eg::DescriptorSet m_descriptorSet;
	bool m_hasUpdatedParametersBuffer = false;
};
