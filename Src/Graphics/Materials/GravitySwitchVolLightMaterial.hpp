#pragma once

#include "../QualityLevel.hpp"

struct GravitySwitchVolLightMaterial : public eg::IMaterial
{
public:
	GravitySwitchVolLightMaterial() = default;

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }

	static void SetQuality(QualityLevel qualityLevel);

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	static eg::MeshBatch::Mesh GetMesh();

	void SetParameters(const glm::vec3& position, const glm::mat3& rotationMatrix, float intensity);

private:
	eg::Buffer m_parametersBuffer;
	eg::DescriptorSet m_descriptorSet;
};
