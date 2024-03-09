#pragma once

#include "../Graphics/GravityGunRenderer.hpp"
#include "../Graphics/Lighting/PointLight.hpp"
#include "Dir.hpp"
#include "Entities/EntGravityChargeable.hpp"

class GravityGun
{
public:
	GravityGun();

	void Update(
		class World& world, const class PhysicsEngine& physicsEngine, class WaterSimulator2* waterSim,
		eg::ParticleManager& particleManager, const class Player& player, const glm::mat4& inverseViewProj, float dt
	);

	void Draw(class RenderTexManager& renderTexManager);

	void ChangeLevel(const glm::quat& oldPlayerRotation, const glm::quat& newPlayerRotation);

	std::shared_ptr<PointLight> light;

	bool shouldShowControlHint = false;

private:
	glm::vec3 m_gunOffset;
	glm::mat4 m_gunTransform;

	float m_bobTime = 0;

	const eg::Model* m_model;

	static constexpr size_t MAX_MESHES_IN_MODEL = 10;

	std::bitset<MAX_MESHES_IN_MODEL> m_meshIsPartOfFront;

	GravityGunRenderer m_gunRenderer;

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
