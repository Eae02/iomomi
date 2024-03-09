#include "GravityGun.hpp"

#include "../Settings.hpp"
#include "../Water/WaterSimulator.hpp"
#include "Player.hpp"
#include "World.hpp"

GravityGun::GravityGun() : m_model(&eg::GetAsset<eg::Model>("Models/GravityGun.obj")), m_gunRenderer(*m_model)
{
	EG_ASSERT(m_model->NumMeshes() < MAX_MESHES_IN_MODEL);

	for (size_t i = 0; i < m_model->NumMeshes(); i++)
	{
		std::string_view name = m_model->GetMesh(i).name;
		m_meshIsPartOfFront[i] = name.starts_with("Front") || name.starts_with("EnergyCyl");
	}

	light = std::make_shared<PointLight>();
	light->enabled = false;
	light->castsShadows = true;
	light->willMoveEveryFrame = true;
}

static glm::vec3 GUN_POSITION(2.44f, -2.16f, -6.5f);
static float GUN_ROTATION_Y = -1.02f;
static float GUN_ROTATION_X = -0.02f;
static float GUN_SCALE = 0.04f;
static float GUN_BOB_SCALE = 0.3f;
static float GUN_BOB_SPEED = 4.0f;

static float ORB_SPEED = 60.0f;

static float LIGHT_INTENSITY_MAX = 20.0f;
static float LIGHT_INTENSITY_FALL_TIME = 0.2f;
static eg::ColorSRGB LIGHT_COLOR = eg::ColorSRGB::FromHex(0xb6fdff);

void GravityGun::SetBeamInstanceTransform(BeamInstance& instance)
{
	instance.particleEmitter.SetTransform(
		glm::translate(glm::mat4(1), instance.beamPos) * glm::mat4(instance.rotationMatrix)
	);
}

void GravityGun::ChangeLevel(const glm::quat& oldPlayerRotation, const glm::quat& newPlayerRotation)
{
	m_gunOffset = newPlayerRotation * glm::inverse(oldPlayerRotation) * m_gunOffset;
}

void GravityGun::Update(
	World& world, const PhysicsEngine& physicsEngine, WaterSimulator2* waterSim, eg::ParticleManager& particleManager,
	const Player& player, const glm::mat4& inverseViewProj, float dt
)
{
	glm::mat3 rotationMatrix = (glm::mat3_cast(player.Rotation()));
	glm::mat3 invRotationMatrix = glm::transpose(rotationMatrix);

	float fovOffsetZ = 0.07f * (settings.fieldOfViewDeg - 80.0f);
	glm::vec3 targetOffset = rotationMatrix * ((GUN_POSITION + glm::vec3(0, 0, fovOffsetZ)) * GUN_SCALE);
	m_gunOffset += (targetOffset - m_gunOffset) * std::min(dt * 30.0f, 1.0f);

	glm::vec3 planeVel = player.Velocity();
	glm::vec3 upAxis = DirectionVector(player.CurrentDown());
	planeVel -= glm::dot(planeVel, upAxis) * upAxis;

	float speed = glm::length(planeVel);
	float xoffset = std::sin(m_bobTime) * speed * GUN_BOB_SCALE / 100.0f;
	float yoffset = std::sin(2.0f * m_bobTime) * speed * GUN_BOB_SCALE / 400.0f;
	m_bobTime += dt * GUN_BOB_SPEED;

	glm::mat4 viewMatrix, viewMatrixInv;
	player.GetViewMatrix(viewMatrix, viewMatrixInv);
	glm::vec3 playerEyePos(viewMatrixInv[3]);

	glm::vec3 translation = glm::vec3(xoffset, yoffset, 0) + invRotationMatrix * m_gunOffset;

	glm::mat4 rotationScaleLocal = glm::rotate(glm::mat4(1.0f), eg::PI * GUN_ROTATION_X, glm::vec3(1, 0, 0)) *
	                               glm::rotate(glm::mat4(1.0f), eg::PI * GUN_ROTATION_Y, glm::vec3(0, 1, 0)) *
	                               glm::scale(glm::mat4(1.0f), glm::vec3(GUN_SCALE));

	m_gunTransform = glm::translate(glm::mat4(), translation) * rotationScaleLocal;

	m_fireAnimationTime = std::max(m_fireAnimationTime - dt, 0.0f);

	// Updates beam instances
	BeamInstance* lightBeamInstance = nullptr;
	for (BeamInstance& beamInstance : m_beamInstances)
	{
		beamInstance.timeRemaining -= dt;
		if (beamInstance.timeRemaining < 0)
		{
			beamInstance.beamPos = beamInstance.targetPos;
			if (std::shared_ptr<EntGravityChargeable> targetEntity = beamInstance.entityToCharge.lock())
			{
				targetEntity->SetGravity(beamInstance.newDown);
			}

			beamInstance.lightIntensity -= dt / LIGHT_INTENSITY_FALL_TIME;
			beamInstance.particleEmitter.Kill();
		}
		else
		{
			beamInstance.beamPos += beamInstance.direction * dt;
			SetBeamInstanceTransform(beamInstance);
		}

		if (lightBeamInstance == nullptr || beamInstance.timeRemaining < lightBeamInstance->timeRemaining)
		{
			lightBeamInstance = &beamInstance;
		}
	}

	light->enabled = settings.gunFlash && lightBeamInstance != nullptr;
	if (lightBeamInstance != nullptr)
	{
		light->position = lightBeamInstance->beamPos;
		light->SetRadiance(LIGHT_COLOR, lightBeamInstance->lightIntensity * LIGHT_INTENSITY_MAX);
	}

	// Removes dead beam instances
	for (int64_t i = eg::ToInt64(m_beamInstances.size()) - 1; i >= 0; i--)
	{
		if (m_beamInstances[i].lightIntensity < 0)
		{
			if (i != eg::ToInt64(m_beamInstances.size()) - 1)
			{
				m_beamInstances.back() = std::move(m_beamInstances[i]);
			}
			m_beamInstances.pop_back();
		}
	}

	const eg::Ray viewRay = eg::Ray::UnprojectNDC(inverseViewProj, glm::vec2(0.0f));
	auto [intersectObject, intersectDist] = physicsEngine.RayIntersect(viewRay, RAY_MASK_BLOCK_GUN);

	shouldShowControlHint = false;
	std::shared_ptr<EntGravityChargeable> entGravityChargeable;
	if (intersectObject && std::holds_alternative<Ent*>(intersectObject->owner))
	{
		entGravityChargeable =
			std::dynamic_pointer_cast<EntGravityChargeable>(std::get<Ent*>(intersectObject->owner)->shared_from_this());
		if (entGravityChargeable)
		{
			shouldShowControlHint = entGravityChargeable->ShouldShowGravityBeamControlHint(player.CurrentDown());
		}
	}

	if (settings.keyShoot.IsDown() && !settings.keyShoot.WasDown() && intersectObject != nullptr)
	{
		BeamInstance& beamInstance = m_beamInstances.emplace_back();

		beamInstance.particleEmitter =
			particleManager.AddEmitter(eg::GetAsset<eg::ParticleEmitterType>("Particles/BlueOrb.ype"));

		glm::vec3 start = playerEyePos + rotationMatrix * translation;
		glm::vec3 target = viewRay.GetPoint(intersectDist * 0.99f);

		beamInstance.newDown = player.CurrentDown();
		beamInstance.direction = glm::normalize(target - start) * ORB_SPEED;
		beamInstance.targetPos = target;
		beamInstance.timeRemaining = intersectDist / ORB_SPEED;
		beamInstance.beamPos = start;
		beamInstance.lightIntensity = 1;
		beamInstance.lightInstanceID = LightSource::NextInstanceID();

		glm::vec3 rotationL = glm::normalize(glm::cross(beamInstance.direction, glm::vec3(0, 1, 0)));
		glm::vec3 rotationU = glm::normalize(glm::cross(beamInstance.direction, rotationL));
		beamInstance.rotationMatrix = glm::mat3(rotationL, rotationU, glm::normalize(beamInstance.direction));

		SetBeamInstanceTransform(beamInstance);

		m_fireAnimationTime = 1;

		beamInstance.entityToCharge = entGravityChargeable;

		if (waterSim != nullptr)
		{
			waterSim->EnqueueGravityChange(viewRay, intersectDist, player.CurrentDown());
		}
	}
}

void GravityGun::Draw(class RenderTexManager& renderTexManager)
{
	constexpr float KICK_BACK_TIME = 0.08f;
	constexpr float KICK_RESTORE_TIME = 0.7f;
	constexpr float KICK_BACK_DIST_MAX = 0.3f;
	constexpr float KICK_BACK_DIST_MAX_FRONT = 0.5f;
	constexpr float ANIMATION_END = 1 - KICK_BACK_TIME - KICK_RESTORE_TIME;

	float kickBackDist = 0;
	if (m_fireAnimationTime > 1 - KICK_BACK_TIME)
	{
		kickBackDist = glm::smoothstep(0.0f, 1.0f, (1 - m_fireAnimationTime) / KICK_BACK_TIME);
	}
	else if (m_fireAnimationTime > ANIMATION_END)
	{
		kickBackDist = glm::smoothstep(0.0f, 1.0f, (m_fireAnimationTime - ANIMATION_END) / (KICK_RESTORE_TIME));
	}

	glm::mat4 meshTransforms[MAX_MESHES_IN_MODEL];
	for (size_t m = 0; m < m_model->NumMeshes(); m++)
	{
		float dstForThisMesh = m_meshIsPartOfFront[m] ? KICK_BACK_DIST_MAX_FRONT : KICK_BACK_DIST_MAX;
		meshTransforms[m] = glm::translate(m_gunTransform, glm::vec3(0, 0, -kickBackDist * dstForThisMesh));
	}

	float glowIntensityBoost = glm::smoothstep(0.0f, 1.0f, m_fireAnimationTime);

	m_gunRenderer.Draw({ meshTransforms, m_model->NumMeshes() }, glowIntensityBoost, renderTexManager);
}
