#include "EntranceExitEnt.hpp"

#include "../../../AudioPlayers.hpp"
#include "../../../Graphics/Lighting/PointLightShadowMapper.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../ImGui.hpp"
#include "../../../Settings.hpp"
#include "../../../Water/WaterSimulator.hpp"
#include "../../Player.hpp"
#include <EntranceEntity.pb.h>

DEF_ENT_TYPE(EntranceExitEnt)

static constexpr float MESH_LENGTH = 4.7f;
static constexpr float MESH_HEIGHT = 3.0f;

static constexpr float DOOR_OPEN_DIST = 2.5f;        // Open the door when the player is closer than this distance
static constexpr float DOOR_CLOSE_DELAY = 0.5f;      // Time in seconds before the door closes after being open
static constexpr float OPEN_TRANSITION_TIME = 0.2f;  // Time in seconds for the open transition
static constexpr float CLOSE_TRANSITION_TIME = 0.3f; // Time in seconds for the close transition

struct
{
	const eg::Model* model;
	std::vector<const eg::IMaterial*> materials;
	std::vector<bool> onlyRenderIfInside;
	std::vector<bool> dynamicShadows;

	int floorLightMaterialIdx;
	int ceilLightMaterialIdx;
	int screenMaterialIdx;

	size_t doorMeshIndices[2][2];

	size_t fanMeshIndex;

	const eg::Model* editorEntModel;
	const eg::Model* editorExitModel;
	const eg::IMaterial* editorEntMaterial;
	const eg::IMaterial* editorExitMaterial;

	eg::CollisionMesh roomCollisionMesh, door1CollisionMesh, door2CollisionMesh;
} entrance;

inline static void AssignMaterial(std::string_view materialName, std::string_view assetName)
{
	int materialIndex = entrance.model->GetMaterialIndex(materialName);
	if (materialIndex == -1)
	{
		EG_PANIC("Material not found: " << materialName);
	}
	entrance.materials[materialIndex] = &eg::GetAsset<StaticPropMaterial>(assetName);
}

static void OnInit()
{
	entrance.model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	entrance.editorEntModel = &eg::GetAsset<eg::Model>("Models/EditorEntrance.aa.obj");
	entrance.editorExitModel = &eg::GetAsset<eg::Model>("Models/EditorExit.aa.obj");

	entrance.materials.resize(
		entrance.model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml")
	);
	AssignMaterial("Floor", "Materials/Entrance/Floor.yaml");
	AssignMaterial("WallPadding", "Materials/Entrance/Padding.yaml");
	AssignMaterial("CeilPipe", "Materials/Pipe2.yaml");
	AssignMaterial("SidePipe", "Materials/Pipe1.yaml");
	AssignMaterial("Door1", "Materials/Entrance/Door1.yaml");
	AssignMaterial("Door2", "Materials/Entrance/Door2.yaml");
	AssignMaterial("DoorFrame", "Materials/Entrance/DoorFrame.yaml");
	AssignMaterial("Walls", "Materials/Entrance/WallPanels.yaml");
	AssignMaterial("Ceiling", "Materials/Entrance/Ceiling.yaml");
	AssignMaterial("Pillars", "Materials/Entrance/Pillars.yaml");
	AssignMaterial("Fan", "Materials/Entrance/Fan.yaml");

	entrance.floorLightMaterialIdx = entrance.model->GetMaterialIndex("FloorLight");
	entrance.ceilLightMaterialIdx = entrance.model->GetMaterialIndex("Light");
	entrance.screenMaterialIdx = entrance.model->GetMaterialIndex("Screen");

	entrance.editorEntMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/EditorEntrance.yaml");
	entrance.editorExitMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/EditorExit.yaml");

	entrance.fanMeshIndex = entrance.model->GetMeshIndex("Fan");

	entrance.onlyRenderIfInside.resize(entrance.model->NumMeshes(), true);
	entrance.dynamicShadows.resize(entrance.model->NumMeshes(), false);
	entrance.dynamicShadows[entrance.fanMeshIndex] = true;

	// Door frames need to be rendered from outside
	int doorFrameMaterialIndex = entrance.model->GetMaterialIndex("DoorFrame");
	for (size_t m = 0; m < entrance.model->NumMeshes(); m++)
	{
		if (entrance.model->GetMesh(m).name.starts_with("Wall") &&
		    entrance.model->GetMesh(m).materialIndex == doorFrameMaterialIndex)
		{
			entrance.onlyRenderIfInside[m] = false;
		}
	}

	for (int d = 0; d < 2; d++)
	{
		for (int h = 0; h < 2; h++)
		{
			char prefix[] = { 'D', 'o', 'o', 'r', d ? '2' : '1', h ? 'B' : 'A' };
			std::string_view prefixSV(prefix, std::size(prefix));

			bool found = false;
			for (size_t m = 0; m < entrance.model->NumMeshes(); m++)
			{
				if (entrance.model->GetMesh(m).name.starts_with(prefixSV))
				{
					entrance.doorMeshIndices[d][h] = m;
					entrance.onlyRenderIfInside[m] = false;
					entrance.dynamicShadows[m] = true;
					found = true;
					break;
				}
			}
			EG_ASSERT(found);
		}
	}

	const eg::Model& colModel = eg::GetAsset<eg::Model>("Models/EnterRoom.col.obj");
	auto MakeCollisionMesh = [&](std::string_view meshName) -> eg::CollisionMesh
	{
		int index = colModel.GetMeshIndex(meshName);
		EG_ASSERT(index != -1);
		return colModel.MakeCollisionMesh(index).value();
	};
	entrance.door1CollisionMesh = MakeCollisionMesh("Door1C");
	entrance.door2CollisionMesh = MakeCollisionMesh("Door2C");
	entrance.roomCollisionMesh = MakeCollisionMesh("WallsC");
}

EG_ON_INIT(OnInit)

std::vector<glm::vec3> EntranceExitEnt::GetConnectionPoints(const Ent& entity)
{
	const EntranceExitEnt& eeent = static_cast<const EntranceExitEnt&>(entity);
	if (eeent.m_type == Type::Entrance)
		return {};

	const glm::mat4 transform = eeent.GetTransform();
	const glm::vec3 connectionPointsLocal[] = { glm::vec3(MESH_LENGTH, 1.0f, -1.5f), glm::vec3(MESH_LENGTH, 1.0f, 1.5f),
		                                        glm::vec3(MESH_LENGTH, 2.5f, 0.0f) };

	std::vector<glm::vec3> connectionPoints;
	for (const glm::vec3& connectionPointLocal : connectionPointsLocal)
	{
		connectionPoints.emplace_back(transform * glm::vec4(connectionPointLocal, 1.0f));
	}
	return connectionPoints;
}

static const eg::ColorLin ScreenTextColor(eg::ColorSRGB::FromHex(0x485156));
static const eg::ColorLin ScreenBackColor(eg::ColorSRGB::FromHex(0x485156));

EntranceExitEnt::EntranceExitEnt()
	: m_activatable(&EntranceExitEnt::GetConnectionPoints),
	  m_pointLight(std::make_shared<PointLight>(glm::vec3(0.0f), eg::ColorSRGB::FromHex(0xfed8d3), 5.0f)),
	  m_fanPointLight(std::make_shared<PointLight>(glm::vec3(0.0f), eg::ColorSRGB::FromHex(0xfed8d3), 60.0f)),
	  m_screenMaterial(500, 200)
{
	m_roomPhysicsObject.canBePushed = false;
	m_door1PhysicsObject.canBePushed = false;
	m_door2PhysicsObject.canBePushed = false;
	m_roomPhysicsObject.shape = &entrance.roomCollisionMesh;
	m_door1PhysicsObject.shape = entrance.door1CollisionMesh.BoundingBox();
	m_door2PhysicsObject.shape = entrance.door2CollisionMesh.BoundingBox();

	m_screenMaterial.render = [this](eg::SpriteBatch& spriteBatch)
	{
		if (m_type == Type::Exit)
			return;
		auto& font = eg::GetAsset<eg::SpriteFont>("GameFont.ttf");
		spriteBatch.DrawText(font, "", glm::vec2(10, 10), ScreenTextColor);
	};

	m_pointLight->highPriority = true;
	m_fanPointLight->highPriority = true;
}

const Dir UP_VECTORS[] = { Dir::PosY, Dir::PosY, Dir::PosX, Dir::PosX, Dir::PosY, Dir::PosY };

std::tuple<glm::mat3, glm::vec3> EntranceExitEnt::GetTransformParts() const
{
	Dir direction = OppositeDir(m_direction);

	glm::vec3 dir = DirectionVector(direction);
	const glm::vec3 up = DirectionVector(UP_VECTORS[static_cast<int>(direction)]);

	const glm::vec3 translation = m_position - up * (MESH_HEIGHT / 2) + dir * MESH_LENGTH;

	if (m_type == Type::Exit)
		dir = -dir;
	return std::make_tuple(glm::mat3(dir, up, glm::cross(dir, up)), translation);
}

glm::mat4 EntranceExitEnt::GetTransform() const
{
	auto [rotation, translation] = GetTransformParts();
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation);
}

glm::mat4 EntranceExitEnt::GetEditorTransform() const
{
	glm::mat4 transform = GetTransform();
	if (m_type == Type::Exit)
	{
		transform *= glm::scale(glm::mat4(), glm::vec3(-1, 1, 1));
	}
	constexpr float Z_FIGHTING_OFFSET = 0.1f;
	transform *= glm::translate(glm::mat4(), glm::vec3(-MESH_LENGTH - Z_FIGHTING_OFFSET, 0, 0));
	return transform;
}

static const glm::vec3 fanRotationCenter = glm::vec3(0, 1.47092f, 2.31192f);

static const glm::vec3 pointLightPos = glm::vec3(0, 2.0f, 1.0f);

static float* fanSpeed = eg::TweakVarFloat("entfan_speed", -2);
static float* fanLightYOffset = eg::TweakVarFloat("entfan_ly", -0.25f);
static float* fanLightZOffset = eg::TweakVarFloat("entfan_lz", 1.1f);

void EntranceExitEnt::Update(const WorldUpdateArgs& args)
{
	if (args.mode != WorldMode::Game)
	{
		m_pointLight->enabled = false;
		m_fanPointLight->enabled = false;
		return;
	}

	Dir direction = OppositeDir(m_direction);

	glm::vec3 toPlayer = args.player->Position() - m_position;
	glm::vec3 openDirToPlayer = DirectionVector(direction) * (m_type == Type::Entrance ? 1 : -1);

	float minDist = (m_doorOpenProgress > 1E-4f) ? -1.0f : 0.0f;

	bool open = m_activatable.AllSourcesActive() &&
	            glm::length2(toPlayer) < DOOR_OPEN_DIST * DOOR_OPEN_DIST && // Player is close to the door
	            glm::dot(toPlayer, openDirToPlayer) > minDist;              // Player is on the right side of the door

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

	// Invalidates shadows if the open progress changed
	if (std::abs(m_doorOpenProgress - oldOpenProgress) > 1E-6f && args.plShadowMapper)
	{
		constexpr float DOOR_RADIUS = 1.5f;
		args.plShadowMapper->Invalidate(eg::Sphere(m_position, DOOR_RADIUS));
	}

	glm::vec3 diag(1.8f);
	diag[static_cast<int>(direction) / 2] = 0.0f;
	glm::vec3 towardsAABBEnd = glm::vec3(DirectionVector(direction)) * MESH_LENGTH * 2.0f;
	eg::AABB targetAABB(m_position - diag, m_position + towardsAABBEnd + diag);
	m_isPlayerInside = targetAABB.Intersects(args.player->GetAABB()) || m_doorOpenProgress != 0;

	m_pointLight->position = GetTransform() * glm::vec4(pointLightPos, 1.0f);
	m_pointLight->enabled = m_isPlayerInside;

	const glm::vec3 fanLightPos(
		fanRotationCenter.x, fanRotationCenter.y + *fanLightYOffset, fanRotationCenter.z + *fanLightZOffset
	);
	m_fanPointLight->position = GetTransform() * glm::vec4(fanLightPos, 1.0f);
	m_fanPointLight->enabled = m_isPlayerInside && settings.shadowQuality >= QualityLevel::Medium;
	if (args.plShadowMapper && m_fanPointLight->enabled)
	{
		glm::vec3 invalidatePos(GetTransform() * glm::vec4(fanRotationCenter, 1));
		args.plShadowMapper->Invalidate(eg::Sphere(invalidatePos, 0.1f), m_fanPointLight.get());
	}

	m_fanRotation += args.dt * *fanSpeed;

	m_shouldSwitchEntrance = m_isPlayerInside && m_doorHasOpened && m_doorOpenProgress == 0 && m_type == Type::Exit &&
	                         glm::dot(toPlayer, openDirToPlayer) < -0.5f;

	bool doorOpen = m_doorOpenProgress > 0.1f;
	m_door1Open = doorOpen && m_type == Type::Exit;
	m_door2Open = doorOpen && m_type == Type::Entrance;
	m_doorHasOpened |= doorOpen;
}

void EntranceExitEnt::GameDraw(const EntGameDrawArgs& args)
{
	std::vector<glm::mat4> transforms(entrance.model->NumMeshes(), GetTransform());

	transforms[entrance.fanMeshIndex] =
		transforms[entrance.fanMeshIndex] * glm::translate(glm::mat4(1), fanRotationCenter) *
		glm::rotate(glm::mat4(1), m_fanRotation, glm::vec3(0, 0, 1)) * glm::translate(glm::mat4(1), -fanRotationCenter);

	float doorMoveDist = glm::smoothstep(0.0f, 1.0f, m_doorOpenProgress) * 1.2f;
	for (int h = 0; h < 2; h++)
	{
		size_t meshIndex = entrance.doorMeshIndices[1 - static_cast<int>(m_type)][h];
		glm::vec3 tVec = glm::vec3(0, 1, -1) * doorMoveDist;
		if (h == 0)
			tVec = -tVec;
		transforms[meshIndex] = transforms[meshIndex] * glm::translate(glm::mat4(1.0f), tVec);
	}

	static const eg::ColorLin FloorLightColor(eg::ColorSRGB::FromHex(0x16aa63));
	static const glm::vec4 FloorLightColorV =
		5.0f * glm::vec4(FloorLightColor.r, FloorLightColor.g, FloorLightColor.b, 1);

	const eg::ColorLin CeilLightColor(m_pointLight->GetColor());
	const glm::vec4 CeilLightColorV = 5.0f * glm::vec4(CeilLightColor.r, CeilLightColor.g, CeilLightColor.b, 1);

	for (size_t i = 0; i < entrance.model->NumMeshes(); i++)
	{
		if (args.shadowDrawArgs)
		{
			if (!args.shadowDrawArgs->renderDynamic && entrance.dynamicShadows[i])
				continue;
			if (!args.shadowDrawArgs->renderStatic && !entrance.dynamicShadows[i])
				continue;
		}

		if (!args.shadowDrawArgs && !m_isPlayerInside && entrance.onlyRenderIfInside[i])
			continue;

		eg::AABB aabb = entrance.model->GetMesh(i).boundingAABB->TransformedBoundingBox(transforms[i]);
		if (!args.frustum->Intersects(aabb))
			continue;

		if (auto materialIndex = entrance.model->GetMesh(i).materialIndex)
		{
			if (materialIndex == entrance.floorLightMaterialIdx)
			{
				args.meshBatch->AddModelMesh(
					*entrance.model, i, EmissiveMaterial::instance,
					EmissiveMaterial::InstanceData{ transforms[i], FloorLightColorV }
				);
			}
			else if (materialIndex == entrance.ceilLightMaterialIdx)
			{
				args.meshBatch->AddModelMesh(
					*entrance.model, i, EmissiveMaterial::instance,
					EmissiveMaterial::InstanceData{ transforms[i], CeilLightColorV }
				);
			}
			else
			{
				const eg::IMaterial* mat = entrance.materials[*materialIndex];
				if (materialIndex == entrance.screenMaterialIdx)
					mat = &m_screenMaterial;

				StaticPropMaterial::InstanceData instanceData(transforms[i]);
				args.meshBatch->AddModelMesh(*entrance.model, i, *mat, instanceData);
			}
		}
	}

	m_levelTitle = args.world->title;
	m_screenMaterial.RenderTexture(ScreenBackColor);
}

void EntranceExitEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	auto model = m_type == Type::Entrance ? entrance.editorEntModel : entrance.editorExitModel;
	auto material = m_type == Type::Entrance ? entrance.editorEntMaterial : entrance.editorExitMaterial;
	args.meshBatch->AddModel(*model, *material, StaticPropMaterial::InstanceData(GetEditorTransform()));
}

void EntranceExitEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();
	ImGui::Separator();
	ImGui::Combo("Type", reinterpret_cast<int*>(&m_type), "Entrance\0Exit\0");
#endif
}

static const float forwardRotations[6] = { eg::PI * 0.5f, eg::PI * 1.5f, 0, 0, eg::PI * 2.0f, eg::PI * 1.0f };

void EntranceExitEnt::InitPlayer(Player& player)
{
	Dir direction = OppositeDir(m_direction);
	player.SetPosition(m_position + glm::vec3(DirectionVector(direction)) * MESH_LENGTH);
	player.m_rotationPitch = 0.0f;
	player.m_rotationYaw = forwardRotations[static_cast<int>(direction)];
}

void EntranceExitEnt::MovePlayer(const EntranceExitEnt& oldExit, const EntranceExitEnt& newEntrance, Player& player)
{
	auto [oldRotation, oldTranslation] = oldExit.GetTransformParts();
	auto [newRotation, newTranslation] = newEntrance.GetTransformParts();

	glm::vec3 posLocal = glm::transpose(oldRotation) * (player.Position() - oldTranslation);
	player.SetPosition(newRotation * posLocal + newTranslation + glm::vec3(0, 0.001f, 0));

	Dir oldDir = OppositeDir(oldExit.m_direction);
	Dir newDir = OppositeDir(newEntrance.m_direction);
	player.m_rotationYaw +=
		-forwardRotations[static_cast<int>(oldDir)] + eg::PI + forwardRotations[static_cast<int>(newDir)];
}

void EntranceExitEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	if (faceDirection)
		m_direction = faceDirection.value();
	m_position = newPosition;
}

Door EntranceExitEnt::GetDoorDescription() const
{
	Dir direction = OppositeDir(m_direction);

	Door door;
	door.position = m_position - glm::vec3(DirectionVector(UP_VECTORS[static_cast<int>(direction)])) * 0.5f;
	door.normal = glm::vec3(DirectionVector(direction));
	door.radius = 1.5f;
	return door;
}

const void* EntranceExitEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return Ent::GetComponent(type);
}

void EntranceExitEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::EntranceEntity entrancePB;

	SerializePos(entrancePB, m_position);

	entrancePB.set_isexit(m_type == Type::Exit);
	entrancePB.set_dir(static_cast<iomomi_pb::Dir>(OppositeDir(m_direction)));
	entrancePB.set_name(m_activatable.m_name);

	entrancePB.SerializeToOstream(&stream);
}

void EntranceExitEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::EntranceEntity entrancePB;
	entrancePB.ParseFromIstream(&stream);

	m_position = DeserializePos(entrancePB);
	m_type = entrancePB.isexit() ? Type::Exit : Type::Entrance;
	m_direction = OppositeDir(static_cast<Dir>(entrancePB.dir()));

	if (entrancePB.name() != 0)
		m_activatable.m_name = entrancePB.name();

	auto [rotation, translation] = GetTransformParts();
	m_roomPhysicsObject.position = m_door1PhysicsObject.position = m_door2PhysicsObject.position = translation;
	m_roomPhysicsObject.rotation = m_door1PhysicsObject.rotation = m_door2PhysicsObject.rotation =
		glm::quat_cast(rotation);
}

void EntranceExitEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_roomPhysicsObject);
	if (!m_door1Open)
		physicsEngine.RegisterObject(&m_door1PhysicsObject);
	if (!m_door2Open)
		physicsEngine.RegisterObject(&m_door2PhysicsObject);
}

void EntranceExitEnt::CollectPointLights(std::vector<std::shared_ptr<PointLight>>& lights)
{
	lights.push_back(m_pointLight);
	lights.push_back(m_fanPointLight);
}

int EntranceExitEnt::EdGetIconIndex() const
{
	return 13 + static_cast<int>(m_type);
}

template <>
std::shared_ptr<Ent> CloneEntity<EntranceExitEnt>(const Ent& entity)
{
	const EntranceExitEnt& original = static_cast<const EntranceExitEnt&>(entity);
	std::shared_ptr<EntranceExitEnt> clone = Ent::Create<EntranceExitEnt>();
	clone->m_type = original.m_type;
	clone->m_position = original.m_position;
	clone->m_direction = original.m_direction;
	return clone;
}
