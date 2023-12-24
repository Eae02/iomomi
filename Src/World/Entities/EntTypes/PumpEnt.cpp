#include "PumpEnt.hpp"

#include "../../../../Protobuf/Build/PumpEntity.pb.h"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../ImGui.hpp"
#include "../../../Settings.hpp"
#include "../../Player.hpp"
#include "../../WorldUpdateArgs.hpp"

DEF_ENT_TYPE(PumpEnt)

static const eg::Model* pumpModel;
static const eg::IMaterial* pumpBodyMaterial;

static eg::CollisionMesh fullCollisionMesh;
static eg::CollisionMesh buttonLCollisionMesh;
static eg::CollisionMesh buttonRCollisionMesh;

static std::vector<PumpDirection> meshButtonPumpDirection;

static int mainMaterialIndex;
static int screenMaterialIndex;
static int emissiveLMaterialIndex;
static int emissiveRMaterialIndex;

static void OnInit()
{
	pumpModel = &eg::GetAsset<eg::Model>("Models/Pump.aa.obj");
	pumpBodyMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Pump.yaml");

	mainMaterialIndex = pumpModel->GetMaterialIndex("Main");
	screenMaterialIndex = pumpModel->GetMaterialIndex("Screen");
	emissiveLMaterialIndex = pumpModel->GetMaterialIndex("EmissiveL");
	emissiveRMaterialIndex = pumpModel->GetMaterialIndex("EmissiveR");

	meshButtonPumpDirection.resize(pumpModel->NumMeshes(), PumpDirection::None);

	std::vector<eg::CollisionMesh> allCollisionMeshes;
	std::vector<eg::CollisionMesh> buttonLCollisionMeshes;
	std::vector<eg::CollisionMesh> buttonRCollisionMeshes;
	for (size_t i = 0; i < pumpModel->NumMeshes(); i++)
	{
		eg::CollisionMesh cm = pumpModel->MakeCollisionMesh(i);
		std::string_view meshName = pumpModel->GetMesh(i).name;
		if (meshName.starts_with("ButtonL"))
		{
			meshButtonPumpDirection[i] = PumpDirection::Left;
			buttonLCollisionMeshes.push_back(cm);
		}
		else if (meshName.starts_with("ButtonR"))
		{
			meshButtonPumpDirection[i] = PumpDirection::Right;
			buttonRCollisionMeshes.push_back(cm);
		}
		allCollisionMeshes.push_back(std::move(cm));
	}

	fullCollisionMesh = eg::CollisionMesh::Join(allCollisionMeshes);
	buttonLCollisionMesh = eg::CollisionMesh::Join(buttonLCollisionMeshes);
	buttonRCollisionMesh = eg::CollisionMesh::Join(buttonRCollisionMeshes);
}

EG_ON_INIT(OnInit)

PumpEnt::PumpEnt()
{
	m_particlesPerSecond = 100;
	m_maxInputDistance = 0.5f;
	m_maxOutputDistance = 0.5f;

	m_editorSelectionMesh.collisionMesh = &fullCollisionMesh;
	m_editorSelectionMesh.model = pumpModel;

	m_physicsObject.shape = fullCollisionMesh.BoundingBox();
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_PICK_UP | RAY_MASK_BLOCK_GUN;
	m_physicsObject.canBePushed = false;
	m_physicsObject.debugColor = 0x12b81a;
	m_physicsObject.owner = this;

	UpdateTransform();
}

void PumpEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
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

	if (ImGui::DragFloat("Pump Rate", &m_particlesPerSecond))
		m_particlesPerSecond = std::max(m_particlesPerSecond, 0.0f);
	if (ImGui::DragFloat("Max Input Distance", &m_maxInputDistance, 0.01f))
		m_maxInputDistance = std::max(m_maxInputDistance, 0.0f);
	if (ImGui::DragFloat("Max Output Distance", &m_maxOutputDistance, 0.01f))
		m_maxOutputDistance = std::max(m_maxOutputDistance, 0.0f);
#endif
}

constexpr float BUTTON_PUSH_ANIMATION_TIME = 0.5f;
constexpr float BUTTON_PUSH_DOWN_TIME_PART = 0.2f;

constexpr float BUTTON_GLOW_ANIMATION_TIME = 0.3f;
constexpr float BUTTON_GLOW_FACTOR_PUSHED = 3.0f;
const eg::ColorLin DISABLED_BUTTON_GLOW_COLOR = PumpScreenMaterial::backgroundColor.ScaleRGB(4.0f);
const eg::ColorLin ENABLED_BUTTON_GLOW_COLOR = PumpScreenMaterial::color.ScaleRGB(8.0f);

constexpr float BUTTON_PUSH_DIST = 0.03f;
const glm::vec3 BUTTON_PUSH_VECTOR = glm::normalize(glm::vec3(0.027997, -0.022140, 0.0f)) * BUTTON_PUSH_DIST;

static float GetButtonAnimationIntensity(float t)
{
	float intensity = t / BUTTON_PUSH_ANIMATION_TIME;
	if (intensity > 1 - BUTTON_PUSH_DOWN_TIME_PART)
		intensity = (1 - intensity) / BUTTON_PUSH_DOWN_TIME_PART;
	else
		intensity /= 1 - BUTTON_PUSH_DOWN_TIME_PART;
	return glm::smoothstep(0.0f, 1.0f, intensity);
}

void PumpEnt::CommonDraw(const EntDrawArgs& args)
{
	float buttonLAnimationIntensity = GetButtonAnimationIntensity(m_buttonLPushTimer);
	float buttonRAnimationIntensity = GetButtonAnimationIntensity(m_buttonRPushTimer);

	for (size_t m = 0; m < pumpModel->NumMeshes(); m++)
	{
		float buttonAnimationIntensity = 0;
		if (meshButtonPumpDirection[m] == PumpDirection::Left)
			buttonAnimationIntensity = buttonLAnimationIntensity;
		else if (meshButtonPumpDirection[m] == PumpDirection::Right)
			buttonAnimationIntensity = buttonRAnimationIntensity;

		glm::mat4 transform = glm::translate(m_transform, BUTTON_PUSH_VECTOR * buttonAnimationIntensity);

		int materialIndex = pumpModel->GetMesh(m).materialIndex;
		if (materialIndex == mainMaterialIndex)
		{
			args.meshBatch->AddModelMesh(*pumpModel, m, *pumpBodyMaterial, StaticPropMaterial::InstanceData(transform));
		}
		else if (materialIndex == screenMaterialIndex)
		{
			args.meshBatch->AddModelMesh(*pumpModel, m, m_screenMaterial, StaticPropMaterial::InstanceData(transform));
		}
		else if (materialIndex == emissiveLMaterialIndex || materialIndex == emissiveRMaterialIndex)
		{
			float activationGlow =
				materialIndex == emissiveLMaterialIndex ? m_buttonLActivationGlow : m_buttonRActivationGlow;

			EmissiveMaterial::InstanceData instanceData;
			instanceData.transform = transform;
			instanceData.color =
				glm::vec4(
					glm::mix(DISABLED_BUTTON_GLOW_COLOR.r, ENABLED_BUTTON_GLOW_COLOR.r, activationGlow),
					glm::mix(DISABLED_BUTTON_GLOW_COLOR.g, ENABLED_BUTTON_GLOW_COLOR.g, activationGlow),
					glm::mix(DISABLED_BUTTON_GLOW_COLOR.b, ENABLED_BUTTON_GLOW_COLOR.b, activationGlow), 0.0f) *
				glm::mix(1.0f, BUTTON_GLOW_FACTOR_PUSHED, buttonAnimationIntensity);
			args.meshBatch->AddModelMesh(*pumpModel, m, EmissiveMaterial::instance, instanceData);
		}
	}
}

void PumpEnt::Update(const WorldUpdateArgs& args)
{
	m_buttonLPushTimer = std::max(m_buttonLPushTimer - args.dt, 0.0f);
	m_buttonRPushTimer = std::max(m_buttonRPushTimer - args.dt, 0.0f);

	const float dt = args.dt / BUTTON_GLOW_ANIMATION_TIME;
	const float targetL = m_currentDirection == PumpDirection::Left ? 1.0f : 0.0f;
	m_buttonLActivationGlow = eg::AnimateTo(m_buttonLActivationGlow, targetL, dt);
	m_buttonRActivationGlow = eg::AnimateTo(m_buttonRActivationGlow, 1 - targetL, dt);

	m_screenMaterial.Update(args.dt, m_currentDirection);
}

void PumpEnt::Interact(Player& player)
{
	switch (GetHoveredButton(player, nullptr))
	{
	case PumpDirection::Left:
		m_currentDirection = PumpDirection::Left;
		m_buttonLPushTimer = BUTTON_PUSH_ANIMATION_TIME;
		break;
	case PumpDirection::Right:
		m_currentDirection = PumpDirection::Right;
		m_buttonRPushTimer = BUTTON_PUSH_ANIMATION_TIME;
		break;
	case PumpDirection::None:
		eg::Log(eg::LogLevel::Error, "", "PumpEnt::GetHoveredButton returned PumpDirection::None");
		break;
	}
}

PumpDirection PumpEnt::GetHoveredButton(const Player& player, const PhysicsEngine* physicsEngine) const
{
	static constexpr float MAX_INTERACT_DIST = 1.75f;

	if (!player.OnGround())
		return PumpDirection::None;

	eg::Ray ray(player.EyePosition(), player.Forward());

	float intersectDistL = INFINITY;
	buttonLCollisionMesh.Intersect(ray, intersectDistL, &m_transform);

	float intersectDistR = INFINITY;
	buttonRCollisionMesh.Intersect(ray, intersectDistR, &m_transform);

	if (intersectDistL > MAX_INTERACT_DIST && intersectDistR > MAX_INTERACT_DIST)
		return PumpDirection::None;

	// Checks if there is another physics object blocking the ray
	if (physicsEngine != nullptr)
	{
		auto [intersectObj, intersectDist] = physicsEngine->RayIntersect(ray, RAY_MASK_BLOCK_PICK_UP, &m_physicsObject);
		if (intersectObj != nullptr && intersectDist < intersectDistL && intersectDist < intersectDistR)
			return PumpDirection::None;
	}

	return intersectDistL < intersectDistR ? PumpDirection::Left : PumpDirection::Right;
}

int PumpEnt::CheckInteraction(const Player& player, const PhysicsEngine& physicsEngine) const
{
	static constexpr int INTERACT_PRIORITY = 2000;
	if (GetHoveredButton(player, &physicsEngine) != PumpDirection::None)
		return INTERACT_PRIORITY;
	return 0;
}

std::optional<InteractControlHint> PumpEnt::GetInteractControlHint() const
{
	InteractControlHint hint;
	hint.keyBinding = &settings.keyInteract;
	hint.message = "Pump Water";
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
	iomomi_pb::PumpEntity entityPB;

	SerializePos(entityPB, m_position);
	SerializeRotation(entityPB, m_rotation);

	entityPB.set_pump_lx(m_leftPumpPoint.x);
	entityPB.set_pump_ly(m_leftPumpPoint.y);
	entityPB.set_pump_lz(m_leftPumpPoint.z);
	entityPB.set_pump_rx(m_rightPumpPoint.x);
	entityPB.set_pump_ry(m_rightPumpPoint.y);
	entityPB.set_pump_rz(m_rightPumpPoint.z);
	entityPB.set_particles_per_second(m_particlesPerSecond);
	entityPB.set_max_input_dist(m_maxInputDistance);
	entityPB.set_max_output_dist(m_maxOutputDistance);

	entityPB.SerializeToOstream(&stream);
}

void PumpEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::PumpEntity entityPB;
	entityPB.ParseFromIstream(&stream);

	m_position = DeserializePos(entityPB);
	m_rotation = DeserializeRotation(entityPB);

	m_leftPumpPoint = glm::vec3(entityPB.pump_lx(), entityPB.pump_ly(), entityPB.pump_lz());
	m_rightPumpPoint = glm::vec3(entityPB.pump_rx(), entityPB.pump_ry(), entityPB.pump_rz());

	m_particlesPerSecond = entityPB.particles_per_second();
	m_maxInputDistance = entityPB.max_input_dist();
	m_maxOutputDistance = entityPB.max_output_dist();

	UpdateTransform();
}

void PumpEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

std::span<const EditorSelectionMesh> PumpEnt::EdGetSelectionMeshes() const
{
	return { &m_editorSelectionMesh, 1 };
}

void PumpEnt::UpdateTransform()
{
	m_physicsObject.position = m_position;
	m_physicsObject.rotation = m_rotation;

	m_transform = glm::translate(glm::mat4(1), m_position) * glm::mat4_cast(m_rotation);
	m_editorSelectionMesh.transform = m_transform;
}

std::optional<WaterPumpDescription> PumpEnt::GetPumpDescription() const
{
	if (m_currentDirection == PumpDirection::None)
		return {};

	WaterPumpDescription desc = {};
	desc.particlesPerSecond = m_particlesPerSecond;
	desc.maxInputDistSquared = m_maxInputDistance * m_maxInputDistance;
	desc.maxOutputDist = m_maxOutputDistance;

	if (m_currentDirection == PumpDirection::Left)
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
