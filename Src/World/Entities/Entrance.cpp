#include "Entrance.hpp"
#include "ECWallMounted.hpp"
#include "ECEditorVisible.hpp"
#include "ECCollidable.hpp"
#include "ECDrawable.hpp"
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
    ECDrawable,
    ECCollidable,
    eg::ECPosition3D
    >();

static eg::EntitySignature lightChildSignature = eg::EntitySignature::Create<eg::ECPosition3D, PointLight>();

static constexpr float MESH_LENGTH = 4.75f;
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
	entrance.materials.at(entrance.model->GetMaterialIndex("Door")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Door1.yaml");
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
}

EG_ON_INIT(OnInit)

const Dir UP_VECTORS[] =
{
	Dir::PosY, Dir::PosY, Dir::PosX, Dir::PosX, Dir::PosY, Dir::PosY
};

inline glm::mat4 GetTransform(eg::Entity& entity)
{
	Dir direction = entity.GetComponent<ECWallMounted>().wallUp;
	
	glm::vec3 dir = DirectionVector(direction);
	const glm::vec3 up = DirectionVector(UP_VECTORS[(int)direction]);
	
	const glm::vec3 translation = eg::GetEntityPosition(entity) - up * (MESH_HEIGHT / 2) + dir * MESH_LENGTH;
	
	if (entity.GetComponent<ECEntrance>().GetType() == ECEntrance::Type::Exit)
		dir = -dir;
	const glm::mat3 rotation(dir, up, glm::cross(dir, up));
	
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation);
}

void ECEntrance::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
	{
		Dir direction = entity.GetComponent<ECWallMounted>().wallUp;
		glm::vec3 position = eg::GetEntityPosition(entity);
		ECEntrance& entranceEC = entity.GetComponent<ECEntrance>();
		
		glm::vec3 toPlayer = args.player->Position() - position;
		glm::vec3 desiredDirToPlayer = DirectionVector(direction) * (entranceEC.m_type == Type::Entrance ? 1 : -1);
		
		const bool open =
			glm::length2(toPlayer) < DOOR_OPEN_DIST * DOOR_OPEN_DIST && //Player is close to the door
			glm::dot(toPlayer, desiredDirToPlayer) > 0 && //Player is on the right side of the door
			args.player->CurrentDown() == OppositeDir(UP_VECTORS[(int)direction]); //Player has the correct gravity mode
		
		entranceEC.m_timeBeforeClose = open ? DOOR_CLOSE_DELAY : std::max(entranceEC.m_timeBeforeClose - args.dt, 0.0f);
		
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
	}
}

void ECEntrance::CalcClipping(eg::Entity& entity, ClippingArgs& args)
{
	const ECEntrance& entranceEC = entity.GetComponent<ECEntrance>();
	glm::mat4 transform = GetTransform(entity);
	
	constexpr float COL_SIZE_X = MESH_LENGTH * 0.99f;
	constexpr float COL_SIZE_Y = 3.0f;
	constexpr float COL_SIZE_Z = 1.5f;
	
	glm::vec3 colMin(-COL_SIZE_X, 0, -COL_SIZE_Z);
	glm::vec3 colMax(COL_SIZE_X, COL_SIZE_Y, COL_SIZE_Z);
	
	glm::vec3 colPolygons[4][4] =
	{
		{ //Floor
			glm::vec3(colMin.x, colMin.y, colMin.z),
			glm::vec3(colMin.x, colMin.y, colMax.z),
			glm::vec3(colMax.x, colMin.y, colMax.z),
			glm::vec3(colMax.x, colMin.y, colMin.z)
		},
		{ //Ceiling
			glm::vec3(colMin.x, colMax.y, colMin.z),
			glm::vec3(colMin.x, colMax.y, colMax.z),
			glm::vec3(colMax.x, colMax.y, colMax.z),
			glm::vec3(colMax.x, colMax.y, colMin.z)
		},
		{ //Left side
			glm::vec3(colMin.x, colMin.y, colMin.z),
			glm::vec3(colMax.x, colMin.y, colMin.z),
			glm::vec3(colMax.x, colMax.y, colMin.z),
			glm::vec3(colMin.x, colMax.y, colMin.z)
		},
		{ //Right side
			glm::vec3(colMin.x, colMin.y, colMax.z),
			glm::vec3(colMax.x, colMin.y, colMax.z),
			glm::vec3(colMax.x, colMax.y, colMax.z),
			glm::vec3(colMin.x, colMax.y, colMax.z)
		}
	};
	
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
			colPolygons[i][j] = glm::vec3(transform * glm::vec4(colPolygons[i][j], 1));
		CalcPolygonClipping(args, colPolygons[i]);
	}
	
	float doorsX[4] = { MESH_LENGTH, 4.3f, -4.3f, -MESH_LENGTH };
	for (int i = 0; i < 4; i++)
	{
		if (entranceEC.m_doorOpenProgress > 0.1f && (i / 2) == (1 - (int)entranceEC.m_type))
			continue;
		
		glm::vec3 doorMin(doorsX[i], 0, -COL_SIZE_Z);
		glm::vec3 doorMax(doorsX[i], COL_SIZE_Y, COL_SIZE_Z);
		
		glm::vec3 doorPolygon[4] =
		{
			glm::vec3(doorMin.x, doorMin.y, doorMin.z),
			glm::vec3(doorMin.x, doorMin.y, doorMax.z),
			glm::vec3(doorMin.x, doorMax.y, doorMax.z),
			glm::vec3(doorMin.x, doorMax.y, doorMin.z)
		};
		for (int j = 0; j < 4; j++)
		{
			doorPolygon[j] = glm::vec3(transform * glm::vec4(doorPolygon[j], 1));
		}
		
		CalcPolygonClipping(args, doorPolygon);
	}
}

void ECEntrance::Draw(eg::Entity& entity, eg::MeshBatch& meshBatch)
{
	const ECEntrance& entranceEC = entity.GetComponent<ECEntrance>();
	
	std::vector<glm::mat4> transforms(entrance.model->NumMeshes(), GetTransform(entity));
	
	float doorMoveDist = glm::smoothstep(0.0f, 1.0f, entranceEC.m_doorOpenProgress) * 1.2f;
	for (int h = 0; h < 2; h++)
	{
		size_t meshIndex = entrance.doorMeshIndices[1 - (int)entranceEC.m_type][h];
		glm::vec3 tVec = glm::vec3(0, 1, -1) * doorMoveDist;
		if (h == 0)
			tVec = -tVec;
		transforms[meshIndex] = transforms[meshIndex] * glm::translate(glm::mat4(1.0f), tVec);
	}
	
	for (size_t i = 0; i < entrance.model->NumMeshes(); i++)
	{
		const size_t materialIndex = entrance.model->GetMesh(i).materialIndex;
		meshBatch.Add(*entrance.model, i, *entrance.materials[materialIndex], transforms[i]);
	}
}

void ECEntrance::EditorDraw(eg::Entity& entity, bool selected, const EditorDrawArgs& drawArgs)
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
	
	drawArgs.meshBatch->Add(*model, *material, transform);
}

void ECEntrance::EditorRenderSettings(eg::Entity& entity)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	ImGui::Separator();
	
	ImGui::Combo("Type", reinterpret_cast<int*>(&entity.GetComponent<ECEntrance>().m_type), "Entrance\0Exit\0");
}

void ECEntrance::InitPlayer(eg::Entity& entity, Player& player)
{
	Dir direction = entity.GetComponent<ECWallMounted>().wallUp;
	
	player.SetPosition(eg::GetEntityPosition(entity) + glm::vec3(DirectionVector(direction)) * MESH_LENGTH);
	
	float rotations[6][2] =
	{
		{ eg::PI * 0.5f, 0 },
		{ eg::PI * 1.5f, 0 },
		{ 0, eg::HALF_PI },
		{ 0, -eg::HALF_PI },
		{ eg::PI * 2.0f, 0 },
		{ eg::PI * 1.0f, 0 }
	};
	
	player.SetRotation(rotations[(int)direction][0], rotations[(int)direction][1]);
}

Door ECEntrance::GetDoorDescription(const eg::Entity& entity)
{
	Dir direction = entity.GetComponent<ECWallMounted>().wallUp;
	
	Door door;
	door.position = eg::GetEntityPosition(entity) - glm::vec3(DirectionVector(UP_VECTORS[(int)direction])) * 0.5f;
	door.normal = glm::vec3(DirectionVector(direction));
	door.radius = 1.5f;
	return door;
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
		
		entrancePB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		entrancePB.set_posx(pos.x);
		entrancePB.set_posy(pos.y);
		entrancePB.set_posz(pos.z);
		
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
		entity.GetComponent<ECWallMounted>().wallUp = (Dir)entrancePB.dir();
		
		eg::Entity& lightChild = eg::Deref(entity.FindChildBySignature(lightChildSignature));
		lightChild.InitComponent<eg::ECPosition3D>(glm::vec3(GetTransform(entity) * glm::vec4(0, 2.0f, 1.0f, 1.0f)) - position);
	}
};

eg::IEntitySerializer* ECEntrance::EntitySerializer = new EntranceSerializer;

eg::Entity* ECEntrance::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Entrance/Exit", &ECEntrance::EditorDraw, &ECEntrance::EditorRenderSettings);
	entity.GetComponent<ECCollidable>().SetCallback(&ECEntrance::CalcClipping);
	entity.GetComponent<ECDrawable>().SetCallback(&ECEntrance::Draw);
	
	eg::Entity& lightChild = entityManager.AddEntity(lightChildSignature, &entity);
	lightChild.InitComponent<PointLight>(eg::ColorSRGB::FromHex(0xDEEEFD), 20.0f);
	
	return &entity;
}