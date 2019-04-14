#pragma once

struct GravitySwitchVolLightMaterial : public eg::IMaterial
{
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	static eg::MeshBatch::Mesh GetMesh();
	
	glm::vec3 switchPosition;
	glm::mat3 rotationMatrix;
};
