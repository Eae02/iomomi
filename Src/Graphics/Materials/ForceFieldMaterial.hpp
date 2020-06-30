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
	
	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }
	
private:
	eg::Buffer m_meshBuffer;
	eg::Pipeline m_pipeline;
	eg::Sampler m_particleSampler;
	eg::DescriptorSet m_descriptorSet;
};
