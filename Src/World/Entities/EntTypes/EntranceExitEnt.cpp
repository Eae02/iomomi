#include "EntranceExitEnt.hpp"
#include "../../Clipping.hpp"
#include "../../BulletPhysics.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Lighting/PointLight.hpp"
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
	
	size_t doorMeshIndices[2][2];
	
	const eg::Model* editorEntModel;
	const eg::Model* editorExitModel;
	const eg::IMaterial* editorEntMaterial;
	const eg::IMaterial* editorExitMaterial;
	
	eg::CollisionMesh door1CollisionMesh;
	eg::CollisionMesh door2CollisionMesh;
	eg::CollisionMesh roomCollisionMesh;
	
	std::unique_ptr<btTriangleMesh> bulletMesh;
	std::unique_ptr<btBvhTriangleMeshShape> bulletMeshShape;
} entrance;

static void OnInit()
{
	entrance.model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	entrance.editorEntModel = &eg::GetAsset<eg::Model>("Models/EditorEntrance.obj");
	entrance.editorExitModel = &eg::GetAsset<eg::Model>("Models/EditorExit.obj");
	
	entrance.materials.resize(entrance.model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	auto AssignMaterial = [&] (std::string_view materialName, std::string_view assetName)
	{
		int materialIndex = entrance.model->GetMaterialIndex(materialName);
		entrance.materials.at(materialIndex) = &eg::GetAsset<StaticPropMaterial>(assetName);
	};
	AssignMaterial("Floor",       "Materials/Entrance/Floor.yaml");
	AssignMaterial("WallPadding", "Materials/Entrance/Padding.yaml");
	AssignMaterial("WallPadding", "Materials/Entrance/Padding.yaml");
	AssignMaterial("CeilPipe",    "Materials/Pipe2.yaml");
	AssignMaterial("Door1",       "Materials/Entrance/Door1.yaml");
	AssignMaterial("Door2",       "Materials/Entrance/Door2.yaml");
	AssignMaterial("DoorFrame",   "Materials/Entrance/DoorFrame.yaml");
	
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
	auto MakeCollisionMesh = [&] (std::string_view name) -> eg::CollisionMesh
	{
		int index = colModel.GetMeshIndex(name);
		EG_ASSERT(index != -1);
		auto meshData = colModel.GetMeshData<eg::StdVertex, uint32_t>(index);
		return eg::CollisionMesh::Create(meshData.vertices, meshData.indices);
	};
	entrance.door1CollisionMesh = MakeCollisionMesh("Door1");
	entrance.door2CollisionMesh = MakeCollisionMesh("Door2");
	entrance.roomCollisionMesh = MakeCollisionMesh("Room");
	
	entrance.bulletMesh = std::make_unique<btTriangleMesh>();
	bullet::AddCollisionMesh(*entrance.bulletMesh, entrance.door1CollisionMesh);
	bullet::AddCollisionMesh(*entrance.bulletMesh, entrance.door2CollisionMesh);
	bullet::AddCollisionMesh(*entrance.bulletMesh, entrance.roomCollisionMesh);
	entrance.bulletMeshShape = std::make_unique<btBvhTriangleMeshShape>(entrance.bulletMesh.get(), true);
}

EG_ON_INIT(OnInit)

std::vector<glm::vec3> EntranceExitEnt::GetConnectionPoints(const Ent& entity)
{
	const glm::mat4 transform = static_cast<const EntranceExitEnt&>(entity).GetTransform();
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

EntranceExitEnt::EntranceExitEnt()
	: m_activatable(&EntranceExitEnt::GetConnectionPoints) { }

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
	Dir direction = OppositeDir(m_direction);
	
	glm::vec3 toPlayer = args.player->Position() - m_position;
	glm::vec3 openDirToPlayer = DirectionVector(direction) * (m_type == Type::Entrance ? 1 : -1);
	
	float minDist = (m_doorOpenProgress > 1E-4f) ? -1.0f : 0.0f;
	
	bool open = m_activatable.AllSourcesActive() &&
		glm::length2(toPlayer) < DOOR_OPEN_DIST * DOOR_OPEN_DIST && //Player is close to the door
		glm::dot(toPlayer, openDirToPlayer) > minDist && //Player is on the right side of the door
		args.player->CurrentDown() == OppositeDir(UP_VECTORS[(int)direction]); //Player has the correct gravity mode
	
	m_timeBeforeClose = open ? DOOR_CLOSE_DELAY : std::max(m_timeBeforeClose - args.dt, 0.0f);
	
	glm::mat4 transform = EntranceExitEnt::GetTransform();
	
	btTransform newBtTransform;
	newBtTransform.setFromOpenGLMatrix(reinterpret_cast<float*>(&transform));
	//entity.GetComponent<ECRigidBody>().SetWorldTransform(newBtTransform);
	
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
}

void EntranceExitEnt::Draw(const EntDrawArgs& args)
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
	
	for (size_t i = 0; i < entrance.model->NumMeshes(); i++)
	{
		const size_t materialIndex = entrance.model->GetMesh(i).materialIndex;
		args.meshBatch->AddModelMesh(*entrance.model, i, *entrance.materials[materialIndex], transforms[i]);
	}
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
	
	args.meshBatch->AddModel(*model, *material, transform);
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
	player.SetRotation(forwardRotations[(int)direction], 0.0f);
}

void EntranceExitEnt::MovePlayer(const EntranceExitEnt& oldExit, const EntranceExitEnt& newEntrance, Player& player)
{
	auto [oldRotation, oldTranslation] = oldExit.GetTransformParts();
	auto [newRotation, newTranslation] = newEntrance.GetTransformParts();
	
	glm::vec3 posLocal = glm::transpose(oldRotation) * (player.Position() - oldTranslation);
	player.SetPosition(newRotation * posLocal + newTranslation + glm::vec3(0, 0.001f, 0));
	
	Dir oldDir = OppositeDir(oldExit.m_direction);
	Dir newDir = OppositeDir(newEntrance.m_direction);
	float yaw = player.RotationYaw() - forwardRotations[(int)oldDir] + eg::PI + forwardRotations[(int)newDir];
	player.SetRotation(yaw, player.RotationPitch());
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
	
	SerializePos(entrancePB);
	
	entrancePB.set_isexit(m_type == Type::Exit);
	entrancePB.set_dir((gravity_pb::Dir)OppositeDir(m_direction));
	entrancePB.set_name(m_activatable.m_name);
	
	entrancePB.SerializeToOstream(&stream);
}

void EntranceExitEnt::Deserialize(std::istream& stream)
{
	gravity_pb::EntranceEntity entrancePB;
	entrancePB.ParseFromIstream(&stream);
	
	DeserializePos(entrancePB);
	m_type = entrancePB.isexit() ? Type::Exit : Type::Entrance;
	m_direction = OppositeDir((Dir)entrancePB.dir());
	
	//eg::Entity& lightChild = eg::Deref(entity.FindChildBySignature(lightChildSignature));
	//lightChild.InitComponent<eg::ECPosition3D>(glm::vec3(GetTransform(entity) * glm::vec4(0, 2.0f, 1.0f, 1.0f)) - position);
	
	if (entrancePB.name() != 0)
		m_activatable.m_name = entrancePB.name();
}

void EntranceExitEnt::CalculateCollision(Dir currentDown, ClippingArgs& args) const
{
	glm::mat4 transform = GetTransform();
	
	auto CheckMesh = [&] (const eg::CollisionMesh& mesh)
	{
		eg::CheckEllipsoidMeshCollision(args.collisionInfo, args.ellipsoid, args.move, mesh, transform);
	};
	
	CheckMesh(entrance.roomCollisionMesh);
	
	bool doorOpen = m_doorOpenProgress > 0.1f;
	if (!(doorOpen && m_type == Type::Entrance))
		CheckMesh(entrance.door2CollisionMesh);
	if (!(doorOpen && m_type == Type::Exit))
		CheckMesh(entrance.door1CollisionMesh);
}

std::pair<bool, float> EntranceExitEnt::RayIntersect(const eg::Ray& ray) const
{
	return std::make_pair(false, 0.0f);
}
