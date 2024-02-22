#pragma once

enum class FFMode : uint32_t
{
	Box = 0,
	Particle = 1
};

struct ForceFieldInstance
{
	FFMode mode;
	glm::vec3 offset;
	glm::vec4 transformX;
	glm::vec4 transformY;
};

class ForceFieldMaterial : public eg::IMaterial
{
public:
	ForceFieldMaterial();

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }

	static constexpr uint32_t POSITION_BINDING = 0;
	static constexpr uint32_t INSTANCE_DATA_BINDING = 1;

private:
	eg::Pipeline m_pipelineBeforeWater;
	eg::Pipeline m_pipelineFinal;

	eg::Buffer m_meshBuffer;
	eg::Sampler m_particleSampler;
};
