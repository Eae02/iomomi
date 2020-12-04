#include "EntranceExitEnt.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../YAMLUtils.hpp"
#include "../../../../Protobuf/Build/EntranceEntity.pb.h"

#include <imgui.h>

static constexpr float MESH_LENGTH = 4.7f;
static constexpr float MESH_HEIGHT = 3.0f;

static constexpr float DOOR_OPEN_DIST = 2.5f; //Open the door when the player is closer than this distance
static constexpr float DOOR_CLOSE_DELAY = 0.5f; //Time in seconds before the door closes after being open
static constexpr float OPEN_TRANSITION_TIME = 0.2f; //Time in seconds for the open transition
static constexpr float CLOSE_TRANSITION_TIME = 0.3f; //Time in seconds for the close transition

struct
{
	const eg::Model* model;
	std::vector<const eg::IMaterial*> materials;
	
	int floorLightMaterialIdx;
	int ceilLightMaterialIdx;
	int screenMaterialIdx;
	
	size_t doorMeshIndices[2][2];
	
	const eg::Model* editorEntModel;
	const eg::Model* editorExitModel;
	const eg::IMaterial* editorEntMaterial;
	const eg::IMaterial* editorExitMaterial;
	
	eg::CollisionMesh roomCollisionMesh, door1CollisionMesh, door2CollisionMesh;
} entrance;

inline static void AssignMaterial(std::string_view materialName, std::string_view assetName)
{
	int materialIndex = entrance.model->GetMaterialIndex(materialName);
	if (materialIndex == -1) {
		EG_PANIC("Material not found: " << materialName);
	}
	entrance.materials[materialIndex] = &eg::GetAsset<StaticPropMaterial>(assetName);
}

static void OnInit()
{
	entrance.model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	entrance.editorEntModel = &eg::GetAsset<eg::Model>("Models/EditorEntrance.obj");
	entrance.editorExitModel = &eg::GetAsset<eg::Model>("Models/EditorExit.obj");
	
	entrance.materials.resize(entrance.model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	AssignMaterial("Floor",       "Materials/Entrance/Floor.yaml");
	AssignMaterial("WallPadding", "Materials/Entrance/Padding.yaml");
	AssignMaterial("CeilPipe",    "Materials/Pipe2.yaml");
	AssignMaterial("SidePipe",    "Materials/Pipe1.yaml");
	AssignMaterial("Door1",       "Materials/Entrance/Door1.yaml");
	AssignMaterial("Door2",       "Materials/Entrance/Door2.yaml");
	AssignMaterial("DoorFrame",   "Materials/Entrance/DoorFrame.yaml");
	
	entrance.floorLightMaterialIdx = entrance.model->GetMaterialIndex("FloorLight");
	entrance.ceilLightMaterialIdx = entrance.model->GetMaterialIndex("Light");
	entrance.screenMaterialIdx = entrance.model->GetMaterialIndex("Screen");
	
	entrance.editorEntMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/EditorEntrance.yaml");
	entrance.editorExitMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/EditorExit.yaml");
	
	for (int d = 0; d < 2; d++)
	{
		for (int h = 0; h < 2; h++)
		{
			char prefix[] = { 'D', 'o', 'o', 'r', d ? '2' : '1', h ? 'B' : 'A' };
			std::string_view prefixSV(prefix, eg::ArrayLen(prefix));
			
			bool found = false;
			for (size_t m = 0; m < entrance.model->NumMeshes(); m++)
			{
				if (eg::StringStartsWith(entrance.model->GetMesh(m).name, prefixSV))
				{
					entrance.doorMeshIndices[d][h] = m;
					found = true;
					break;
				}
			}
			EG_ASSERT(found);
		}
	}
	
	const eg::Model& colModel = eg::GetAsset<eg::Model>("Models/EnterRoomCol.obj");
	auto MakeCollisionMesh = [&] (std::string_view meshName) -> eg::CollisionMesh
	{
		int index = colModel.GetMeshIndex(meshName);
		EG_ASSERT(index != -1);
		auto meshData = colModel.GetMeshData<eg::StdVertex, uint32_t>(index);
		return eg::CollisionMesh::Create(meshData.vertices, meshData.indices);
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
		return { };
	
	const glm::mat4 transform = eeent.GetTransform();
	const glm::vec3 connectionPointsLocal[] = 
	{
		glm::vec3(MESH_LENGTH, 1.0f, -1.5f),
		glm::vec3(MESH_LENGTH, 1.0f, 1.5f),
		glm::vec3(MESH_LENGTH, 2.5f, 0.0f)
	};
	
	std::vector<glm::vec3> connectionPoints;
	for (const glm::vec3& connectionPointLocal : connectionPointsLocal)
	{
		connectionPoints.emplace_back(transform * glm::vec4(connectionPointLocal, 1.0f));
	}
	return connectionPoints;
}

static const eg::ColorLin ScreenTextColor(eg::ColorSRGB::FromHex(0x0b1a21));
static const eg::ColorLin ScreenBackColor(eg::ColorSRGB::FromHex(0xd8f3ff));

EntranceExitEnt::EntranceExitEnt()
	: m_activatable(&EntranceExitEnt::GetConnectionPoints),
	  m_pointLight(eg::ColorSRGB::FromHex(0xD1F8FE), 20.0f),
	  m_screenMaterial(500, 200)
{
	m_roomPhysicsObject.canBePushed = false;
	m_door1PhysicsObject.canBePushed = false;
	m_door2PhysicsObject.canBePushed = false;
	m_roomPhysicsObject.shape = &entrance.roomCollisionMesh;
	m_door1PhysicsObject.shape = entrance.door1CollisionMesh.BoundingBox();
	m_door2PhysicsObject.shape = entrance.door2CollisionMesh.BoundingBox();
	
	m_screenMaterial.render = [this] (eg::SpriteBatch& spriteBatch)
	{
		if (m_type == Type::Exit)
			return;
		auto& font = eg::GetAsset<eg::SpriteFont>("GameFont.ttf");
		spriteBatch.DrawText(font, m_levelTitle, glm::vec2(10, 10), ScreenTextColor);
	};
}

const Dir UP_VECTORS[] =
{
	Dir::PosY, Dir::PosY, Dir::PosX, Dir::PosX, Dir::PosY, Dir::PosY
};

std::tuple<glm::mat3, glm::vec3> EntranceExitEnt::GetTransformParts() const
{
	Dir direction = OppositeDir(m_direction);
	
	glm::vec3 dir = DirectionVector(direction);
	const glm::vec3 up = DirectionVector(UP_VECTORS[(int)direction]);
	
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

void EntranceExitEnt::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	Dir direction = OppositeDir(m_direction);
	
	glm::vec3 toPlayer = args.player->Position() - m_position;
	glm::vec3 openDirToPlayer = DirectionVector(direction) * (m_type == Type::Entrance ? 1 : -1);
	
	float minDist = (m_doorOpenProgress > 1E-4f) ? -1.0f : 0.0f;
	
	bool open = m_activatable.AllSourcesActive() &&
		glm::length2(toPlayer) < DOOR_OPEN_DIST * DOOR_OPEN_DIST && //Player is close to the door
		glm::dot(toPlayer, openDirToPlayer) > minDist && //Player is on the right side of the door
		args.player->CurrentDown() == OppositeDir(UP_VECTORS[(int)direction]); //Player has the correct gravity mode
	
	m_timeBeforeClose = open ? DOOR_CLOSE_DELAY : std::max(m_timeBeforeClose - args.dt, 0.0f);
	
	//Updates the door open progress
	const float oldOpenProgress = m_doorOpenProgress;
	if (m_timeBeforeClose > 0)
	{
		//Door should be open
		m_doorOpenProgress = std::min(m_doorOpenProgress + args.dt / OPEN_TRANSITION_TIME, 1.0f);
	}
	else
	{
		//Door should be closed
		m_doorOpenProgress = std::max(m_doorOpenProgress - args.dt / CLOSE_TRANSITION_TIME, 0.0f);
	}
	
	//Invalidates shadows if the open progress changed
	if (std::abs(m_doorOpenProgress - oldOpenProgress) > 1E-6f && args.invalidateShadows)
	{
		constexpr float DOOR_RADIUS = 1.5f;
		args.invalidateShadows(eg::Sphere(m_position, DOOR_RADIUS));
	}
	
	m_shouldSwitchEntrance = false;
	if (m_type == Type::Exit && m_doorOpenProgress == 0 && glm::dot(toPlayer, openDirToPlayer) < -0.5f)
	{
		glm::vec3 diag(1.8f);
		diag[(int)direction / 2] = 0.0f;
		
		glm::vec3 midEnd = m_position + glm::vec3(DirectionVector(direction)) * MESH_LENGTH * 2.0f;
		
		eg::AABB targetAABB(m_position - diag, midEnd + diag);
		
		if (targetAABB.Intersects(args.player->GetAABB()))
			m_shouldSwitchEntrance = true;
	}
	
	bool doorOpen = m_doorOpenProgress > 0.1f;
	m_door1Open = doorOpen && m_type == Type::Exit;
	m_door2Open = doorOpen && m_type == Type::Entrance;
}

void EntranceExitEnt::GameDraw(const EntGameDrawArgs& args)
{
	std::vector<glm::mat4> transforms(entrance.model->NumMeshes(), GetTransform());
	
	float doorMoveDist = glm::smoothstep(0.0f, 1.0f, m_doorOpenProgress) * 1.2f;
	for (int h = 0; h < 2; h++)
	{
		size_t meshIndex = entrance.doorMeshIndices[1 - (int)m_type][h];
		glm::vec3 tVec = glm::vec3(0, 1, -1) * doorMoveDist;
		if (h == 0)
			tVec = -tVec;
		transforms[meshIndex] = transforms[meshIndex] * glm::translate(glm::mat4(1.0f), tVec);
	}
	
	static const eg::ColorLin FloorLightColor(eg::ColorSRGB::FromHex(0x16aa63));
	static const glm::vec4 FloorLightColorV = 5.0f * glm::vec4(FloorLightColor.r, FloorLightColor.g, FloorLightColor.b, 1);
	
	const eg::ColorLin CeilLightColor(m_pointLight.GetColor());
	const glm::vec4 CeilLightColorV = 5.0f * glm::vec4(CeilLightColor.r, CeilLightColor.g, CeilLightColor.b, 1);
	
	for (size_t i = 0; i < entrance.model->NumMeshes(); i++)
	{
		int materialIndex = entrance.model->GetMesh(i).materialIndex;
		if (materialIndex == entrance.floorLightMaterialIdx)
		{
			args.meshBatch->AddModelMesh(*entrance.model, i, EmissiveMaterial::instance,
				EmissiveMaterial::InstanceData { transforms[i], FloorLightColorV });
		}
		else if (materialIndex == entrance.ceilLightMaterialIdx)
		{
			args.meshBatch->AddModelMesh(*entrance.model, i, EmissiveMaterial::instance,
				EmissiveMaterial::InstanceData { transforms[i], CeilLightColorV });
		}
		else
		{
			const eg::IMaterial* mat = entrance.materials[materialIndex];
			if (materialIndex == entrance.screenMaterialIdx)
				mat = &m_screenMaterial;
			
			StaticPropMaterial::InstanceData instanceData(transforms[i]);
			args.meshBatch->AddModelMesh(*entrance.model, i, *mat, instanceData);
		}
	}
	
	args.pointLights->emplace_back(m_pointLight.GetDrawData(glm::vec3(GetTransform() * glm::vec4(0, 2.0f, 1.0f, 1.0f))));
	
	m_levelTitle = args.world->title;
	m_screenMaterial.RenderTexture(ScreenBackColor);
}

void EntranceExitEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	glm::mat4 transform = GetTransform();
	
	if (m_type == Type::Exit)
	{
		transform *= glm::scale(glm::mat4(), glm::vec3(-1, 1, 1));
	}
	transform *= glm::translate(glm::mat4(), glm::vec3(-MESH_LENGTH - 0.01f, 0, 0));
	
	auto model = m_type == Type::Entrance ? entrance.editorEntModel : entrance.editorExitModel;
	auto material = m_type == Type::Entrance ? entrance.editorEntMaterial : entrance.editorExitMaterial;
	
	args.meshBatch->AddModel(*model, *material, StaticPropMaterial::InstanceData(transform));
}

void EntranceExitEnt::RenderSettings()
{
	Ent::RenderSettings();
	ImGui::Separator();
	ImGui::Combo("Type", reinterpret_cast<int*>(&m_type), "Entrance\0Exit\0");
}

static const float forwardRotations[6] = { eg::PI * 0.5f, eg::PI * 1.5f, 0, 0, eg::PI * 2.0f, eg::PI * 1.0f };

void EntranceExitEnt::InitPlayer(Player& player)
{
	Dir direction = OppositeDir(m_direction);
	player.SetPosition(m_position + glm::vec3(DirectionVector(direction)) * MESH_LENGTH);
	player.m_rotationPitch = 0.0f;
	player.m_rotationYaw = forwardRotations[(int)direction];
}

void EntranceExitEnt::MovePlayer(const EntranceExitEnt& oldExit, const EntranceExitEnt& newEntrance, Player& player)
{
	auto [oldRotation, oldTranslation] = oldExit.GetTransformParts();
	auto [newRotation, newTranslation] = newEntrance.GetTransformParts();
	
	glm::vec3 posLocal = glm::transpose(oldRotation) * (player.Position() - oldTranslation);
	player.SetPosition(newRotation * posLocal + newTranslation + glm::vec3(0, 0.001f, 0));
	
	Dir oldDir = OppositeDir(oldExit.m_direction);
	Dir newDir = OppositeDir(newEntrance.m_direction);
	player.m_rotationYaw += -forwardRotations[(int)oldDir] + eg::PI + forwardRotations[(int)newDir];
}

void EntranceExitEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	if (faceDirection)
		m_direction = faceDirection.value();
	m_position = newPosition;
}

Door EntranceExitEnt::GetDoorDescription() const
{
	Dir direction = OppositeDir(m_direction);
	
	Door door;
	door.position = m_position - glm::vec3(DirectionVector(UP_VECTORS[(int)direction])) * 0.5f;
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
	gravity_pb::EntranceEntity entrancePB;
	
	SerializePos(entrancePB, m_position);
	
	entrancePB.set_isexit(m_type == Type::Exit);
	entrancePB.set_dir((gravity_pb::Dir)OppositeDir(m_direction));
	entrancePB.set_name(m_activatable.m_name);
	
	entrancePB.SerializeToOstream(&stream);
}

void EntranceExitEnt::Deserialize(std::istream& stream)
{
	gravity_pb::EntranceEntity entrancePB;
	entrancePB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(entrancePB);
	m_type = entrancePB.isexit() ? Type::Exit : Type::Entrance;
	m_direction = OppositeDir((Dir)entrancePB.dir());
	
	if (entrancePB.name() != 0)
		m_activatable.m_name = entrancePB.name();
	
	auto [rotation, translation] = GetTransformParts();
	m_roomPhysicsObject.position = m_door1PhysicsObject.position = m_door2PhysicsObject.position = translation;
	m_roomPhysicsObject.rotation = m_door1PhysicsObject.rotation = m_door2PhysicsObject.rotation = glm::quat_cast(rotation);
}

void EntranceExitEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_roomPhysicsObject);
	if (!m_door1Open)
		physicsEngine.RegisterObject(&m_door1PhysicsObject);
	if (!m_door2Open)
		physicsEngine.RegisterObject(&m_door2PhysicsObject);
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
