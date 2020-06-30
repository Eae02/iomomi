#pragma once

#include "../QualityLevel.hpp"

struct GravitySwitchVolLightMaterial : public eg::IMaterial
{
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }
	
	static void SetQuality(QualityLevel qualityLevel);
	
	static eg::MeshBatch::Mesh GetMesh();
	
	glm::vec3 switchPosition;
	glm::mat3 rotationMatrix;
	float intensity = 1;
};
