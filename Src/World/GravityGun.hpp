#pragma once

#include "Dir.hpp"

struct GravityChargeSetMessage : eg::Message<GravityChargeSetMessage>
{
	Dir newDown;
	bool* set;
};

struct GravityChargeResetMessage : eg::Message<GravityChargeResetMessage>
{
	
};

class GravityGun
{
public:
	GravityGun();
	
	void Update(class World& world, class WaterSimulator& waterSim, eg::ParticleManager& particleManager,
		const class Player& player, const glm::mat4& inverseViewProj, float dt);
	
	void Draw(eg::MeshBatch& meshBatch);
	
private:
	struct MidMaterial : eg::IMaterial
	{
		MidMaterial();
		
		size_t PipelineHash() const override;
		bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
		bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
		
		eg::Pipeline m_pipeline;
		eg::DescriptorSet m_descriptorSet;
	};
	
	glm::vec3 m_gunOffset;
	glm::mat4 m_gunTransform;
	
	float m_bobTime = 0;
	
	MidMaterial m_midMaterial;
	
	const eg::Model* m_model;
	std::array<const eg::IMaterial*, 2> m_materials;
};
