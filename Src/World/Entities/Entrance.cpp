#include "Entrance.hpp"
#include "ECWallMounted.hpp"
#include "ECEditorVisible.hpp"
#include "ECActivatable.hpp"
#include "ECRigidBody.hpp"
#include "../Player.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"
#include "../../YAMLUtils.hpp"
#include "../../../Protobuf/Build/EntranceEntity.pb.h"

#include <imgui.h>

eg::EntitySignature ECEntrance::EntitySignature = eg::EntitySignature::Create<
    ECEntrance,
    ECWallMounted,
    ECEditorVisible,
    ECActivatable,
	ECRigidBody,
    eg::ECPosition3D
    >();

eg::MessageReceiver ECEntrance::MessageReceiver = eg::MessageReceiver::Create<ECEntrance,
    CalculateCollisionMessage, DrawMessage, EditorRenderImGuiMessage, EditorDrawMessage>();

static eg::EntitySignature lightChildSignature = eg::EntitySignature::Create<eg::ECPosition3D, PointLight>();

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

void OnInit()
{
	entrance.model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	entrance.editorEntModel = &eg::GetAsset<eg::Model>("Models/EditorEntrance.obj");
	entrance.editorExitModel = &eg::GetAsset<eg::Model>("Models/EditorExit.obj");
	
	entrance.materials.resize(entrance.model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	entrance.materials.at(entrance.model->GetMaterialIndex("Floor")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Floor.yaml");
	entrance.materials.at(entrance.model->GetMaterialIndex("WallPadding")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Padding.yaml");
	entrance.materials.at(entrance.model->GetMaterialIndex("WallPadding")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Padding.yaml");
	entrance.materials.at(entrance.model->GetMaterialIndex("CeilPipe")) = &eg::GetAsset<StaticPropMaterial>("Materials/Pipe2.yaml");
	entrance.materials.at(entrance.model->GetMaterialIndex("Door1")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Door1.yaml");
	entrance.materials.at(entrance.model->GetMaterialIndex("Door2")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Door2.yaml");
	entrance.materials.at(entrance.model->GetMaterialIndex("DoorFrame")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/DoorFrame.yaml");
	
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

const Dir UP_VECTORS[] =
{
	Dir::PosY, Dir::PosY, Dir::PosX, Dir::PosX, Dir::PosY, Dir::PosY
};

inline std::tuple<glm::mat3, glm::vec3> GetTransformParts(const eg::Entity& entity)
{
	Dir direction = OppositeDir(entity.GetComponent<ECWallMounted>().wallUp);
	
	glm::vec3 dir = DirectionVector(direction);
	const glm::vec3 up = DirectionVector(UP_VECTORS[(int)direction]);
	
	const glm::vec3 translation = eg::GetEntityPosition(entity) - up * (MESH_HEIGHT / 2) + dir * MESH_LENGTH;
	
	if (entity.GetComponent<ECEntrance>().GetType() == ECEntrance::Type::Exit)
		dir = -dir;
	return std::make_tuple(glm::mat3(dir, up, glm::cross(dir, up)), translation);
}

inline glm::mat4 GetTransform(const eg::Entity& entity)
{
	auto [rotation, translation] = GetTransformParts(entity);
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation);
}

void ECEntrance::Update(const WorldUpdateArgs& args, eg::Entity** switchEntranceOut)
{
	for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
	{
		Dir direction = OppositeDir(entity.GetComponent<ECWallMounted>().wallUp);
		glm::vec3 position = eg::GetEntityPosition(entity);
		ECEntrance& entranceEC = entity.GetComponent<ECEntrance>();
		ECActivatable& activatableEC = entity.GetComponent<ECActivatable>();
		
		glm::vec3 toPlayer = args.player->Position() - position;
		glm::vec3 openDirToPlayer = DirectionVector(direction) * (entranceEC.m_type == Type::Entrance ? 1 : -1);
		
		float minDist = (entranceEC.m_doorOpenProgress > 1E-4f) ? -1.0f : 0.0f;
		
		bool open = activatableEC.AllSourcesActive() &&
			glm::length2(toPlayer) < DOOR_OPEN_DIST * DOOR_OPEN_DIST && //Player is close to the door
			glm::dot(toPlayer, openDirToPlayer) > minDist && //Player is on the right side of the door
			args.player->CurrentDown() == OppositeDir(UP_VECTORS[(int)direction]); //Player has the correct gravity mode
		
		entranceEC.m_timeBeforeClose = open ? DOOR_CLOSE_DELAY : std::max(entranceEC.m_timeBeforeClose - args.dt, 0.0f);
		
		glm::mat4 transform = GetTransform(entity);
		
		btTransform newBtTransform;
		newBtTransform.setFromOpenGLMatrix(reinterpret_cast<float*>(&transform));
		entity.GetComponent<ECRigidBody>().SetWorldTransform(newBtTransform);
		
		//Updates the door open progress
		const float oldOpenProgress = entranceEC.m_doorOpenProgress;
		if (entranceEC.m_timeBeforeClose > 0)
		{
			//Door should be open
			entranceEC.m_doorOpenProgress = std::min(entranceEC.m_doorOpenProgress + args.dt / OPEN_TRANSITION_TIME, 1.0f);
		}
		else
		{
			//Door should be closed
			entranceEC.m_doorOpenProgress = std::max(entranceEC.m_doorOpenProgress - args.dt / CLOSE_TRANSITION_TIME, 0.0f);
		}
		
		//Invalidates shadows if the open progress changed
		if (std::abs(entranceEC.m_doorOpenProgress - oldOpenProgress) > 1E-6f && args.invalidateShadows)
		{
			constexpr float DOOR_RADIUS = 1.5f;
			args.invalidateShadows(eg::Sphere(position, DOOR_RADIUS));
		}
		
		if (entranceEC.m_type == Type::Exit && entranceEC.m_doorOpenProgress == 0 &&
		    glm::dot(toPlayer, openDirToPlayer) < -0.5f && switchEntranceOut != nullptr)
		{
			glm::vec3 diag(1.8f);
			diag[(int)direction / 2] = 0.0f;
			
			glm::vec3 midEnd = position + glm::vec3(DirectionVector(direction)) * MESH_LENGTH * 2.0f;
			
			eg::AABB targetAABB(position - diag, midEnd + diag);
			
			if (targetAABB.Intersects(args.player->GetAABB()))
			{
				*switchEntranceOut = &entity;
			}
		}
	}
}

void ECEntrance::HandleMessage(eg::Entity& entity, const CalculateCollisionMessage& message)
{
	glm::mat4 transform = GetTransform(entity);
	
	auto CheckMesh = [&] (const eg::CollisionMesh& mesh)
	{
		eg::CheckEllipsoidMeshCollision(message.clippingArgs->collisionInfo, message.clippingArgs->ellipsoid,
			message.clippingArgs->move, mesh, transform);
	};
	
	CheckMesh(entrance.roomCollisionMesh);
	
	bool doorOpen = m_doorOpenProgress > 0.1f;
	if (!(doorOpen && m_type == Type::Entrance))
		CheckMesh(entrance.door2CollisionMesh);
	if (!(doorOpen && m_type == Type::Exit))
		CheckMesh(entrance.door1CollisionMesh);
}

void ECEntrance::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	std::vector<glm::mat4> transforms(entrance.model->NumMeshes(), GetTransform(entity));
	
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
		message.meshBatch->AddModelMesh(*entrance.model, i, *entrance.materials[materialIndex], transforms[i]);
	}
}

void ECEntrance::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	const ECEntrance& entranceEC = entity.GetComponent<ECEntrance>();
	glm::mat4 transform = GetTransform(entity);
	
	if (entranceEC.m_type == Type::Exit)
	{
		transform *= glm::scale(glm::mat4(), glm::vec3(-1, 1, 1));
	}
	transform *= glm::translate(glm::mat4(), glm::vec3(-MESH_LENGTH - 0.01f, 0, 0));
	
	auto model = entranceEC.m_type == Type::Entrance ? entrance.editorEntModel : entrance.editorExitModel;
	auto material = entranceEC.m_type == Type::Entrance ? entrance.editorEntMaterial : entrance.editorExitMaterial;
	
	message.meshBatch->AddModel(*model, *material, transform);
}

void ECEntrance::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	ImGui::Separator();
	
	ImGui::Combo("Type", reinterpret_cast<int*>(&entity.GetComponent<ECEntrance>().m_type), "Entrance\0Exit\0");
}

const float forwardRotations[6] =
{
	eg::PI * 0.5f,
	eg::PI * 1.5f,
	0,
	0,
	eg::PI * 2.0f,
	eg::PI * 1.0f
};

void ECEntrance::InitPlayer(eg::Entity& entity, Player& player)
{
	Dir direction = OppositeDir(entity.GetComponent<ECWallMounted>().wallUp);
	
	player.SetPosition(eg::GetEntityPosition(entity) + glm::vec3(DirectionVector(direction)) * MESH_LENGTH);
	
	player.SetRotation(forwardRotations[(int)direction], 0.0f);
}

void ECEntrance::MovePlayer(const eg::Entity& oldExit, const eg::Entity& newEntrance, Player& player)
{
	auto [oldRotation, oldTranslation] = GetTransformParts(oldExit);
	auto [newRotation, newTranslation] = GetTransformParts(newEntrance);
	
	glm::vec3 posLocal = glm::transpose(oldRotation) * (player.Position() - oldTranslation);
	player.SetPosition(newRotation * posLocal + newTranslation + glm::vec3(0, 0.001f, 0));
	
	Dir oldDir = OppositeDir(oldExit.GetComponent<ECWallMounted>().wallUp);
	Dir newDir = OppositeDir(newEntrance.GetComponent<ECWallMounted>().wallUp);
	float yaw = player.RotationYaw() - forwardRotations[(int)oldDir] + eg::PI + forwardRotations[(int)newDir];
	player.SetRotation(yaw, player.RotationPitch());
}

Door ECEntrance::GetDoorDescription(const eg::Entity& entity)
{
	Dir direction = OppositeDir(entity.GetComponent<ECWallMounted>().wallUp);
	
	Door door;
	door.position = eg::GetEntityPosition(entity) - glm::vec3(DirectionVector(UP_VECTORS[(int)direction])) * 0.5f;
	door.normal = glm::vec3(DirectionVector(direction));
	door.radius = 1.5f;
	return door;
}

static std::vector<glm::vec3> GetConnectionPoints(const eg::Entity& entity)
{
	const glm::mat4 transform = GetTransform(entity);
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

struct EntranceSerializer : public eg::IEntitySerializer
{
	std::string_view GetName() const override
	{
		return "Entrance";
	}
	
	void Serialize(const eg::Entity& entity, std::ostream& stream) const override
	{
		gravity_pb::EntranceEntity entrancePB;
		
		entrancePB.set_isexit(entity.GetComponent<ECEntrance>().GetType() == ECEntrance::Type::Exit);
		
		entrancePB.set_dir((gravity_pb::Dir)OppositeDir(entity.GetComponent<ECWallMounted>().wallUp));
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		entrancePB.set_posx(pos.x);
		entrancePB.set_posy(pos.y);
		entrancePB.set_posz(pos.z);
		
		const ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		entrancePB.set_name(activatable.Name());
		entrancePB.set_reqactivations(activatable.EnabledConnections());
		
		entrancePB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECEntrance::CreateEntity(entityManager);
		
		gravity_pb::EntranceEntity entrancePB;
		entrancePB.ParseFromIstream(&stream);
		
		ECEntrance& entranceEC = entity.GetComponent<ECEntrance>();
		entranceEC.SetType(entrancePB.isexit() ? ECEntrance::Type::Exit : ECEntrance::Type::Entrance);
		
		glm::vec3 position(entrancePB.posx(), entrancePB.posy(), entrancePB.posz());
		entity.InitComponent<eg::ECPosition3D>(position);
		entity.GetComponent<ECWallMounted>().wallUp = OppositeDir((Dir)entrancePB.dir());
		
		eg::Entity& lightChild = eg::Deref(entity.FindChildBySignature(lightChildSignature));
		lightChild.InitComponent<eg::ECPosition3D>(glm::vec3(GetTransform(entity) * glm::vec4(0, 2.0f, 1.0f, 1.0f)) - position);
		
		ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		if (entrancePB.name() != 0)
			activatable.SetName(entrancePB.name());
		activatable.SetEnabledConnections(entrancePB.reqactivations());
	}
};

eg::IEntitySerializer* ECEntrance::EntitySerializer = new EntranceSerializer;

eg::Entity* ECEntrance::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Entrance/Exit");
	entity.InitComponent<ECActivatable>(&GetConnectionPoints);
	
	eg::Entity& lightChild = entityManager.AddEntity(lightChildSignature, &entity);
	lightChild.InitComponent<PointLight>(eg::ColorSRGB::FromHex(0xDEEEFD), 20.0f);
	
	ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
	rigidBody.InitStatic(*entrance.bulletMeshShape);
	
	return &entity;
}
