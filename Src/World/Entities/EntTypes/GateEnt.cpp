#include "GateEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Player.hpp"
#include "../../WorldUpdateArgs.hpp"

#include <GateEntity.pb.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <magic_enum/magic_enum.hpp>
#include <misc/cpp/imgui_stdlib.h>

DEF_ENT_TYPE(GateEnt)

static constexpr size_t NUM_GATE_VARIANTS = magic_enum::enum_count<GateVariant>();

static const eg::Model* doorModel;

static eg::CollisionMesh collisionMesh;

struct GateVariantDescription
{
	std::vector<const eg::IMaterial*> materials;
};

static std::array<GateVariantDescription, NUM_GATE_VARIANTS> variantDescriptions;

static void InitGateVariantDescription(GateVariant variant, std::string_view materialName)
{
	std::vector<const eg::IMaterial*> materials(doorModel->NumMaterials());

	for (size_t i = 0; i < doorModel->NumMaterials(); i++)
	{
		materials[i] = &eg::GetAsset<StaticPropMaterial>(materialName);
	}

	int doorFrameMaterialIndex = doorModel->GetMaterialIndex("DoorFrame");
	if (doorFrameMaterialIndex != -1)
		materials[doorFrameMaterialIndex] = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/DoorFrame.yaml");

	variantDescriptions.at(static_cast<int>(variant)) = GateVariantDescription{
		.materials = std::move(materials),
	};
}

static void OnInit()
{
	doorModel = &eg::GetAsset<eg::Model>("Models/Door.glb");

	auto& collisionModel = eg::GetAsset<eg::Model>("Models/Door.col.glb");
	collisionMesh = collisionModel.MakeCollisionMesh().value();

	InitGateVariantDescription(GateVariant::Neutral, "Materials/Default.yaml");
	InitGateVariantDescription(GateVariant::LevelExit, "Materials/Default.yaml");
}

EG_ON_INIT(OnInit)

static std::atomic_uint32_t nextInstanceID;

GateEnt::GateEnt() : m_activatable(&GateEnt::GetConnectionPoints)
{
	m_physicsObject.canBePushed = false;
	m_physicsObject.shape = &collisionMesh;

	m_doorPhysicsObject.canBePushed = false;
	m_doorPhysicsObject.shape = eg::AABB(glm::vec3(-0.3f, 0.0f, -1.1f), glm::vec3(0.3f, 2.3f, 1.1f));

	m_instanceID = nextInstanceID++;
}

constexpr float DOOR_X_OFFSET = 0.3f;

std::vector<glm::vec3> GateEnt::GetConnectionPoints(const Ent& entity)
{
	const GateEnt& eeent = static_cast<const GateEnt&>(entity);

	const glm::mat4 transform = eeent.GetTransform();
	const glm::vec3 connectionPointsLocal[] = {
		glm::vec3(-DOOR_X_OFFSET, 1.0f, -1.5f),
		glm::vec3(-DOOR_X_OFFSET, 1.0f, 1.5f),
		glm::vec3(-DOOR_X_OFFSET, 2.5f, 0.0f),
	};

	std::vector<glm::vec3> connectionPoints;
	for (const glm::vec3& connectionPointLocal : connectionPointsLocal)
	{
		connectionPoints.emplace_back(transform * glm::vec4(connectionPointLocal, 1.0f));
	}
	return connectionPoints;
}

void GateEnt::RenderSettings()
{
	Ent::RenderSettings();
#ifdef EG_HAS_IMGUI

	ImGui::Separator();

	auto variantNames = magic_enum::enum_names<GateVariant>();

	if (ImGui::BeginCombo("Variant", variantNames[static_cast<int>(variant)].data()))
	{
		for (int i = 0; i < (int)variantNames.size(); i++)
		{
			GateVariant v = static_cast<GateVariant>(i);
			if (ImGui::Selectable(variantNames[i].data(), variant == v))
			{
				variant = v;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::InputText("Name", &name);
#endif
}

void GateEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	if (faceDirection)
		direction = faceDirection.value();
	position = newPosition;
}

static constexpr float DOOR_CLOSE_DELAY = 0.2f;      // Time in seconds before the door closes after being open
static constexpr float OPEN_TRANSITION_TIME = 0.15f; // Time in seconds for the open transition
static constexpr float CLOSE_TRANSITION_TIME = 0.3f; // Time in seconds for the close transition
static constexpr float DOOR_MOVE_DIST = 1.3f;        // How far the door should move when opened

const Dir UP_VECTORS[] = { Dir::PosY, Dir::PosY, Dir::PosX, Dir::PosX, Dir::PosY, Dir::PosY };

void GateEnt::MovePlayer(const GateEnt* oldGate, const GateEnt& newGate, Player& player)
{
	static const float forwardRotations[6] = { eg::PI * 0.5f, eg::PI * 1.5f, 0, 0, eg::PI * 2.0f, eg::PI * 1.0f };

	if (oldGate == nullptr)
	{
		player.SetPosition(newGate.position - glm::vec3(DirectionVector(newGate.direction)) * 2.0f);
		player.m_rotationPitch = 0.0f;
		player.m_rotationYaw = forwardRotations[static_cast<int>(OppositeDir(newGate.direction))];
	}
	else
	{
		auto [oldTranslation, oldRotation] = oldGate->GetTranslationAndRotation();
		auto [newTranslation, newRotation] = newGate.GetTranslationAndRotation();

		glm::vec3 posLocal = glm::transpose(oldRotation) * (player.Position() - oldTranslation);
		posLocal.x = 6.0f - DOOR_X_OFFSET - posLocal.x;
		posLocal.z = -posLocal.z;
		player.SetPosition(newRotation * posLocal + newTranslation + glm::vec3(0, 0.001f, 0));

		Dir oldDir = OppositeDir(oldGate->direction);
		Dir newDir = OppositeDir(newGate.direction);
		player.m_rotationYaw +=
			-forwardRotations[static_cast<int>(oldDir)] + eg::PI + forwardRotations[static_cast<int>(newDir)];
	}
}

void GateEnt::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
	{
		m_isPlayerInside = false;
		m_canLoadNext = false;
		return;
	}

	auto [translation, rotation] = GetTranslationAndRotation();
	glm::vec3 playerPosLocal = glm::transpose(rotation) * (args.player->Position() - translation);

	m_isPlayerInside = eg::AABB(glm::vec3(0, 0, -2), glm::vec3(6, 2, 2)).Contains(playerPosLocal);

	float distToPlayerYZ = glm::length(glm::vec2(playerPosLocal.y - 1.0f, playerPosLocal.z));

	if (playerPosLocal.x < -2.5f)
		m_doorOpenedSide = -1;
	if (playerPosLocal.x > 2.5f)
		m_doorOpenedSide = 1;

	std::pair<float, float> openPlayerBoundsZSide = { -2.5f, 2.5f };
	if (m_doorOpenedSide == 1)
		openPlayerBoundsZSide.first = -1.0f;
	if (m_doorOpenedSide == -1)
		openPlayerBoundsZSide.second = 1.0f;

	bool open = m_activatable.AllSourcesActive() && distToPlayerYZ < 2.5f &&
	            playerPosLocal.x < openPlayerBoundsZSide.second && playerPosLocal.x > openPlayerBoundsZSide.first;

	m_isPlayerCloseWithWrongGravity = false;
	if (open && args.player->CurrentDown() != OppositeDir(UP_VECTORS[static_cast<int>(direction)]))
	{
		m_isPlayerCloseWithWrongGravity = true;
		open = false;
	}

	m_canLoadNext = m_isPlayerInside && playerPosLocal.x > 3.5f && m_doorOpenProgress <= 0.0f;

	m_isPlayerCloseWithWrongGravity = false;
	if (open && args.player->CurrentDown() != OppositeDir(UP_VECTORS[static_cast<int>(direction)]))
	{
		m_isPlayerCloseWithWrongGravity = true;
		open = false;
	}

	m_timeBeforeClose = open ? DOOR_CLOSE_DELAY : std::max(m_timeBeforeClose - args.dt, 0.0f);

	// Updates the door open progress
	const float oldOpenProgress = m_doorOpenProgress;
	if (m_timeBeforeClose > 0)
	{
		// Door should be open
		m_doorOpenProgress = std::min(m_doorOpenProgress + args.dt / OPEN_TRANSITION_TIME, 1.0f);
	}
	else
	{
		// Door should be closed
		m_doorOpenProgress = std::max(m_doorOpenProgress - args.dt / CLOSE_TRANSITION_TIME, 0.0f);
	}

	m_doorPhysicsObject.shouldCollide = (m_doorOpenProgress > 0.5f) ? &ShouldCollideNever : nullptr;

	// Invalidates shadows if the open progress changed
	if (std::abs(m_doorOpenProgress - oldOpenProgress) > 1E-6f && args.plShadowMapper)
	{
		constexpr float DOOR_RADIUS = 1.5f;
		args.plShadowMapper->Invalidate(eg::Sphere(position, DOOR_RADIUS));
	}
}

void GateEnt::CommonDraw(const EntDrawArgs& args)
{
	if (args.shadowDrawArgs && !args.shadowDrawArgs->renderDynamic)
		return;

	const auto& variantDesc = variantDescriptions.at(static_cast<int>(variant));

	glm::mat4 transform = GetTransform();

	auto DrawMesh = [&](std::string_view meshName, const glm::mat4& meshTransform)
	{
		int meshIndex = doorModel->GetMeshIndex(meshName);
		if (meshIndex == -1)
			return;
		std::optional<size_t> materialIndex = doorModel->GetMesh(meshIndex).materialIndex;
		if (!materialIndex.has_value())
			return;

		StaticPropMaterial::InstanceData instanceData(meshTransform);
		args.meshBatch->AddModelMesh(*doorModel, meshIndex, *variantDesc.materials.at(*materialIndex), instanceData);
	};

	DrawMesh("Frame", transform);
	DrawMesh("InterRoom", transform);

	const float doorTranslationDist = glm::smoothstep(0.0f, 1.0f, m_doorOpenProgress) * DOOR_MOVE_DIST;
	DrawMesh("DoorN", transform * glm::translate(glm::mat4(1), glm::vec3(0, 0, doorTranslationDist)));
	DrawMesh("DoorP", transform * glm::translate(glm::mat4(1), glm::vec3(0, 0, -doorTranslationDist)));

	glm::mat4 deepDoorTransform = transform * glm::translate(glm::mat4(1), glm::vec3(6.0f, 0, 0));
	DrawMesh("Frame", deepDoorTransform);
	DrawMesh("DoorN", deepDoorTransform);
	DrawMesh("DoorP", deepDoorTransform);
}

Door GateEnt::GetDoorDescription() const
{
	Dir odir = OppositeDir(direction);

	Door door;
	door.position = position - glm::vec3(DirectionVector(UP_VECTORS[static_cast<int>(odir)])) * 0.5f;
	door.normal = glm::vec3(DirectionVector(odir));
	door.radius = 1.5f;
	return door;
}

glm::mat4 GateEnt::GetTransform() const
{
	auto [translation, rotation] = GetTranslationAndRotation();
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation);
}

std::pair<glm::vec3, glm::mat3> GateEnt::GetTranslationAndRotation() const
{
	glm::vec3 dir = DirectionVector(OppositeDir(direction));
	const glm::vec3 up = DirectionVector(UP_VECTORS[static_cast<int>(direction)]);

	const glm::vec3 translation = position - up * 1.5f + dir * DOOR_X_OFFSET;

	const auto rotation = glm::mat3(dir, up, glm::cross(dir, up));

	return std::make_pair(translation, rotation);
}

const void* GateEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return Ent::GetComponent(type);
}

void GateEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::GateEntity entityPB;
	SerializePos(entityPB, position);

	entityPB.set_dir(static_cast<iomomi_pb::Dir>(direction));
	entityPB.set_variant(static_cast<int>(variant));
	entityPB.set_gate_name(name);
	entityPB.set_activatable_name(m_activatable.m_name);

	entityPB.SerializeToOstream(&stream);
}

void GateEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::GateEntity entityPB;
	entityPB.ParseFromIstream(&stream);

	position = DeserializePos(entityPB);
	direction = static_cast<Dir>(entityPB.dir());
	variant = static_cast<GateVariant>(entityPB.variant());
	name = entityPB.gate_name();

	if (entityPB.activatable_name() != 0)
		m_activatable.m_name = entityPB.activatable_name();

	auto [translation, rotation] = GetTranslationAndRotation();
	m_physicsObject.position = translation;
	m_physicsObject.rotation = glm::quat(rotation);

	m_doorPhysicsObject.position = translation;
	m_doorPhysicsObject.rotation = glm::quat(rotation);
}

int GateEnt::EdGetIconIndex() const
{
	return 5;
}

void GateEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
	physicsEngine.RegisterObject(&m_doorPhysicsObject);
}
