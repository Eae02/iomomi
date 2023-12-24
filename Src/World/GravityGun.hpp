#pragma once

#include "../Graphics/Lighting/PointLight.hpp"
#include "Dir.hpp"
#include "Entities/EntGravityChargeable.hpp"

class GravityGun
{
public:
	GravityGun();

	void Update(
		class World& world, const class PhysicsEngine& physicsEngine, class IWaterSimulator* waterSim,
		eg::ParticleManager& particleManager, const class Player& player, const glm::mat4& inverseViewProj, float dt);

	void Draw(eg::MeshBatch& meshBatch, eg::MeshBatchOrdered& transparentMeshBatch);

	void ChangeLevel(const glm::quat& oldPlayerRotation, const glm::quat& newPlayerRotation);

	std::shared_ptr<PointLight> light;

	bool shouldShowControlHint = false;

private:
	struct MidMaterial : eg::IMaterial
	{
		MidMaterial();

		size_t PipelineHash() const override;
		bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
		bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
		OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }

		eg::Pipeline m_pipeline;
		eg::DescriptorSet m_descriptorSet;
		float m_intensityBoost = 0;
	};

	glm::vec3 m_gunOffset;
	glm::mat4 m_gunTransform;

	float m_bobTime = 0;

	MidMaterial m_midMaterial;

	const eg::Model* m_model;
	std::array<const eg::IMaterial*, 2> m_materials;

	std::bitset<10> m_meshIsPartOfFront;

	float m_fireAnimationTime = 0;

	struct BeamInstance
	{
		eg::ParticleEmitterInstance particleEmitter;
		glm::vec3 direction;
		glm::vec3 targetPos;
		glm::vec3 beamPos;
		float timeRemaining = 0;
		uint64_t lightInstanceID;
		float lightIntensity = 0;
		Dir newDown;
		glm::mat3 rotationMatrix;
		std::weak_ptr<EntGravityChargeable> entityToCharge;
	};

	static void SetBeamInstanceTransform(BeamInstance& instance);

	std::vector<BeamInstance> m_beamInstances;
};
