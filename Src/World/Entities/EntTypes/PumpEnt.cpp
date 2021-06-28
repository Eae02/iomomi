#include "PumpEnt.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../Settings.hpp"
#include "../../../../Protobuf/Build/PumpEntity.pb.h"

#include <imgui.h>

static const eg::Model* pumpModel;
static const eg::IMaterial* pumpBodyMaterial;
static int bodyMeshIndex;
static int screenMeshIndex;

static eg::CollisionMesh collisionMeshes[2];

static void OnInit()
{
	pumpModel = &eg::GetAsset<eg::Model>("Models/Pump.aa.obj");
	pumpBodyMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Pump.yaml");
	bodyMeshIndex = pumpModel->GetMeshIndex("PumpBody");
	screenMeshIndex = pumpModel->GetMeshIndex("PumpScreen");
	EG_ASSERT(pumpModel->NumMeshes() == 2 && bodyMeshIndex != -1 && screenMeshIndex != -1);
	
	for (size_t i = 0; i < 2; i++)
	{
		collisionMeshes[i] = pumpModel->MakeCollisionMesh(i);
	}
}

EG_ON_INIT(OnInit)

PumpEnt::PumpEnt()
{
	m_particlesPerSecond = 100;
	m_maxInputDistance = 0.5f;
	m_maxOutputDistance = 0.5f;
	
	for (size_t i = 0; i < 2; i++)
	{
		m_editorSelectionMeshes[i].collisionMesh = &collisionMeshes[i];
		m_editorSelectionMeshes[i].model = pumpModel;
		m_editorSelectionMeshes[i].meshIndex = i;
	}
	
	m_physicsObject.shape = collisionMeshes[bodyMeshIndex].BoundingBox();
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_PICK_UP | RAY_MASK_BLOCK_GUN;
	m_physicsObject.canBePushed = false;
	m_physicsObject.debugColor = 0x12b81a;
	m_physicsObject.owner = this;
	
	UpdateTransform();
}

void PumpEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::Button("Reset Rotation"))
	{
		m_rotation = glm::quat();
	}
	
	ImGui::Separator();
	
	ImGui::DragFloat3("Left Pump Point", &m_leftPumpPoint.x, 0.1f);
	ImGui::DragFloat3("Right Pump Point", &m_rightPumpPoint.x, 0.1f);
	if (ImGui::Button("Swap Left & Right"))
	{
		std::swap(m_leftPumpPoint, m_rightPumpPoint);
	}
	
	ImGui::Separator();
	
	if (ImGui::BeginCombo("Initial Direction", m_pumpLeft ? "Left" : "Right"))
	{
		if (ImGui::Selectable("Left", m_pumpLeft))
			m_pumpLeft = true;
		if (ImGui::Selectable("Right", !m_pumpLeft))
			m_pumpLeft = false;
		ImGui::EndCombo();
	}
	
	if (ImGui::DragFloat("Pump Rate", &m_particlesPerSecond))
		m_particlesPerSecond = std::max(m_particlesPerSecond, 0.0f);
	if (ImGui::DragFloat("Max Input Distance", &m_maxInputDistance, 0.01f))
		m_maxInputDistance = std::max(m_maxInputDistance, 0.0f);
	if (ImGui::DragFloat("Max Output Distance", &m_maxOutputDistance, 0.01f))
		m_maxOutputDistance = std::max(m_maxOutputDistance, 0.0f);
}

void PumpEnt::CommonDraw(const EntDrawArgs& args)
{
	args.meshBatch->AddModelMesh(
		*pumpModel,
		bodyMeshIndex,
		*pumpBodyMaterial,
		StaticPropMaterial::InstanceData(m_transform)
	);
	
	static const eg::ColorLin lightColor(eg::ColorSRGB::FromHex(0xa7dde6));
	static const glm::vec4 lightColorV = 5.0f * glm::vec4(lightColor.r, lightColor.g, lightColor.b, 1);
	
	args.meshBatch->AddModelMesh(
		*pumpModel,
		screenMeshIndex,
		EmissiveMaterial::instance,
		EmissiveMaterial::InstanceData { m_transform, lightColorV }
	);
}

void PumpEnt::Interact(Player& player)
{
	m_pumpLeft = !m_pumpLeft;
	m_hasInteracted = true;
}

int PumpEnt::CheckInteraction(const Player& player, const PhysicsEngine& physicsEngine) const
{
	if (!player.OnGround())
		return false;
	
	static constexpr float MAX_INTERACT_DIST = 1.75f;
	static constexpr int INTERACT_PRIORITY = 2000;
	eg::Ray ray(player.EyePosition(), player.Forward());
	
	float screenMeshIntersectDist = INFINITY;
	if (collisionMeshes[screenMeshIndex].Intersect(ray, screenMeshIntersectDist, &m_transform) == -1)
		return 0;
	if (screenMeshIntersectDist > MAX_INTERACT_DIST)
		return 0;
	
	auto[intersectObj, intersectDist] = physicsEngine.RayIntersect(ray, RAY_MASK_BLOCK_PICK_UP, &m_physicsObject);
	
	if (intersectObj == nullptr || intersectDist > screenMeshIntersectDist)
	{
		return INTERACT_PRIORITY;
	}
	
	return 0;
}

std::optional<InteractControlHint> PumpEnt::GetInteractControlHint() const
{
	InteractControlHint hint;
	hint.keyBinding = &settings.keyInteract;
	hint.message = m_hasInteracted ? "Reverse Direction" : "Pump Water";
	return hint;
}

void PumpEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	UpdateTransform();
}

glm::quat PumpEnt::EdGetRotation() const
{
	return m_rotation;
}

void PumpEnt::EdRotated(const glm::quat& newRotation)
{
	m_rotation = newRotation;
	UpdateTransform();
}

void PumpEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::PumpEntity entityPB;
	
	SerializePos(entityPB, m_position);
	SerializeRotation(entityPB, m_rotation);
	
	entityPB.set_pump_lx(m_leftPumpPoint.x);
	entityPB.set_pump_ly(m_leftPumpPoint.y);
	entityPB.set_pump_lz(m_leftPumpPoint.z);
	entityPB.set_pump_rx(m_rightPumpPoint.x);
	entityPB.set_pump_ry(m_rightPumpPoint.y);
	entityPB.set_pump_rz(m_rightPumpPoint.z);
	entityPB.set_initial_direction(m_pumpLeft);
	entityPB.set_particles_per_second(m_particlesPerSecond);
	entityPB.set_max_input_dist(m_maxInputDistance);
	entityPB.set_max_output_dist(m_maxOutputDistance);
	
	entityPB.SerializeToOstream(&stream);
}

void PumpEnt::Deserialize(std::istream& stream)
{
	gravity_pb::PumpEntity entityPB;
	entityPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(entityPB);
	m_rotation = DeserializeRotation(entityPB);
	
	m_leftPumpPoint = glm::vec3(entityPB.pump_lx(), entityPB.pump_ly(), entityPB.pump_lz());
	m_rightPumpPoint = glm::vec3(entityPB.pump_rx(), entityPB.pump_ry(), entityPB.pump_rz());
	
	m_pumpLeft           = entityPB.initial_direction();
	m_particlesPerSecond = entityPB.particles_per_second();
	m_maxInputDistance   = entityPB.max_input_dist();
	m_maxOutputDistance  = entityPB.max_output_dist();
	
	UpdateTransform();
}

void PumpEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

std::span<const EditorSelectionMesh> PumpEnt::EdGetSelectionMeshes() const
{
	return m_editorSelectionMeshes;
}

void PumpEnt::UpdateTransform()
{
	m_physicsObject.position = m_position;
	m_physicsObject.rotation = m_rotation;
	
	m_transform = glm::translate(glm::mat4(1), m_position) * glm::mat4_cast(m_rotation);
	for (EditorSelectionMesh& selectionMesh : m_editorSelectionMeshes)
	{
		selectionMesh.transform = m_transform;
	}
}

std::optional<WaterPumpDescription> PumpEnt::GetPumpDescription() const
{
	if (!m_hasInteracted)
		return {};
	
	WaterPumpDescription desc = {};
	desc.particlesPerSecond = m_particlesPerSecond;
	desc.maxInputDistSquared = m_maxInputDistance * m_maxInputDistance;
	desc.maxOutputDist = m_maxOutputDistance;
	
	if (m_pumpLeft)
	{
		desc.source = m_rightPumpPoint;
		desc.dest = m_leftPumpPoint;
	}
	else
	{
		desc.source = m_leftPumpPoint;
		desc.dest = m_rightPumpPoint;
	}
	
	return desc;
}
