#include "GravityGun.hpp"
#include "World.hpp"
#include "Player.hpp"
#include "../Graphics/Materials/StaticPropMaterial.hpp"
#include "../Settings.hpp"

#include <imgui.h>

struct ECGravityOrb
{
	glm::vec3 direction;
	float timeAlive;
	Dir newDown;
	eg::EntityHandle entityToCharge;
};

static eg::EntitySignature gravityOrbSignature =
	eg::EntitySignature::Create<ECGravityOrb, eg::ECPosition3D, eg::ECParticleSystem>();

GravityGun::GravityGun()
{
	m_model = &eg::GetAsset<eg::Model>("Models/GravityGun.obj");
	if (m_model->NumMaterials() > m_materials.size())
	{
		EG_PANIC("Too many materials in gravity gun model");
	}
	
	std::fill(m_materials.begin(), m_materials.end(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	
	m_materials.at(m_model->GetMaterialIndex("Main")) = &eg::GetAsset<StaticPropMaterial>("Materials/GravityGunMainA.yaml");
}

static glm::vec3 GUN_POSITION(2.44f, -2.16f, -6.5f);
static float GUN_ROTATION_Y = -1.02f;
static float GUN_ROTATION_X = -0.02f;
static float GUN_SCALE = 0.04f;
static float GUN_BOB_SCALE = 0.3f;
static float GUN_BOB_SPEED = 4.0f;

static float ORB_SPEED = 30.0f;

void GravityGun::Update(World& world, eg::ParticleManager& particleManager, const Player& player,
	const glm::mat4& inverseViewProj, float dt)
{
	glm::mat3 rotationMatrix = (glm::mat3_cast(player.Rotation()));
	
	/*
	if (ImGui::Begin("Weapon"))
	{
		ImGui::DragFloat3("Position", &GUN_POSITION.x, 0.1f);
		ImGui::DragFloat("RotY", &GUN_ROTATION_Y, 0.001f);
		ImGui::DragFloat("RotX", &GUN_ROTATION_X, 0.001f);
		ImGui::DragFloat("Scale", &GUN_SCALE, 0.001f, 0.0f, 1.0f);
		ImGui::DragFloat("Bob Scale", &GUN_BOB_SCALE, 0.001f, 0.0f, 1.0f);
	}
	ImGui::End();
	*/
	
	float fovOffsetZ = 0.07f * (settings.fieldOfViewDeg - 80.0f);
	glm::vec3 targetOffset = rotationMatrix * ((GUN_POSITION + glm::vec3(0, 0, fovOffsetZ)) * GUN_SCALE);
	m_gunOffset += (targetOffset - m_gunOffset) * std::min(dt * 30.0f, 1.0f);
	
	glm::vec3 planeVel = player.Velocity();
	glm::vec3 upAxis = DirectionVector(player.CurrentDown());
	planeVel -= glm::dot(planeVel, upAxis) * upAxis;
	
	float speed = glm::length(planeVel);
	float xoffset = sin(m_bobTime) * speed * GUN_BOB_SCALE / 100;
	float yoffset = sin(2 * m_bobTime) * speed * GUN_BOB_SCALE / 400;
	m_bobTime += dt * GUN_BOB_SPEED;
	
	glm::vec3 translation = (rotationMatrix * glm::vec3(xoffset, yoffset, 0)) + player.EyePosition() + m_gunOffset;
	
	m_gunTransform = glm::translate(glm::mat4(), translation) * glm::mat4(rotationMatrix);
	m_gunTransform = glm::rotate(m_gunTransform, eg::PI * GUN_ROTATION_X, glm::vec3(1, 0, 0));
	m_gunTransform = glm::rotate(m_gunTransform, eg::PI * GUN_ROTATION_Y, glm::vec3(0, 1, 0));
	m_gunTransform = glm::scale(m_gunTransform, glm::vec3(GUN_SCALE));
	
	for (eg::Entity& orbEntity : world.EntityManager().GetEntitySet(gravityOrbSignature))
	{
		ECGravityOrb& orbComp = orbEntity.GetComponent<ECGravityOrb>();
		orbComp.timeAlive -= dt;
		orbEntity.GetComponent<eg::ECPosition3D>().position += orbComp.direction * dt;
		
		if (orbComp.timeAlive < 0)
		{
			if (eg::Entity* targetEntity = orbComp.entityToCharge.Get())
			{
				bool set = false;
				
				GravityChargeSetMessage chargeSetMessage;
				chargeSetMessage.newDown = orbComp.newDown;
				chargeSetMessage.set = &set;
				
				targetEntity->HandleMessage(chargeSetMessage);
			}
			
			orbEntity.Despawn();
		}
	}
	
	if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		eg::Ray viewRay = eg::Ray::UnprojectNDC(inverseViewProj, glm::vec2(0.0f));
		RayIntersectResult intersectResult = world.RayIntersect(viewRay);
		
		if (intersectResult.intersected)
		{
			eg::Entity& orbEntity = world.EntityManager().AddEntity(gravityOrbSignature);
			orbEntity.InitComponent<eg::ECParticleSystem>(&particleManager).AddEmitter(
				eg::GetAsset<eg::ParticleEmitterType>("Particles/BlueOrb.ype"));
			
			ECGravityOrb& orbComp = orbEntity.GetComponent<ECGravityOrb>();
			orbComp.newDown = player.CurrentDown();
			orbComp.direction = viewRay.GetDirection() * ORB_SPEED;
			orbComp.timeAlive = intersectResult.distance / ORB_SPEED;
			if (intersectResult.entity != nullptr)
				orbComp.entityToCharge = *intersectResult.entity;
			
			orbEntity.InitComponent<eg::ECPosition3D>(m_gunTransform * glm::vec4(0, 0, 0, 1));
		}
	}
}

void GravityGun::Draw(eg::MeshBatch& meshBatch)
{
	for (size_t m = 0; m < m_model->NumMeshes(); m++)
	{
		meshBatch.AddModelMesh(*m_model, m, *m_materials[m_model->GetMesh(m).materialIndex], m_gunTransform, -1);
	}
}
