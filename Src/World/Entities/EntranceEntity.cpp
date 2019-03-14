#include "EntranceEntity.hpp"
#include "../Player.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Graphics/ObjectRenderer.hpp"

#include <imgui.h>

constexpr float MESH_LENGTH = 4.8f;
constexpr float MESH_HEIGHT = 3.0f;

static const eg::Model* s_model;
static std::vector<const ObjectMaterial*> s_materials;

static size_t s_doorMeshIndices[2][2];

static const eg::Model* s_editorEntranceModel;
static const eg::Model* s_editorExitModel;
static const ObjectMaterial* s_editorEntranceMaterial;
static const ObjectMaterial* s_editorExitMaterial;

void InitEntranceEntity()
{
	s_model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	s_editorEntranceModel = &eg::GetAsset<eg::Model>("Models/EditorEntrance.obj");
	s_editorExitModel = &eg::GetAsset<eg::Model>("Models/EditorExit.obj");
	
	s_materials.resize(s_model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	s_materials.at(s_model->GetMaterialIndex("Floor")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Floor.yaml");
	s_materials.at(s_model->GetMaterialIndex("WallPadding")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Padding.yaml");
	s_materials.at(s_model->GetMaterialIndex("WallPadding")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Padding.yaml");
	s_materials.at(s_model->GetMaterialIndex("CeilPipe")) = &eg::GetAsset<StaticPropMaterial>("Materials/Pipe2.yaml");
	
	s_editorEntranceMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/EditorEntrance.yaml");
	s_editorExitMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/EditorExit.yaml");
	
	for (int d = 0; d < 2; d++)
	{
		for (int h = 0; h < 2; h++)
		{
			char prefix[] = { 'D', 'o', 'o', 'r', d ? '2' : '1', h ? 'B' : 'A' };
			std::string_view prefixSV(prefix, eg::ArrayLen(prefix));
			
			bool found = false;
			for (size_t m = 0; m < s_model->NumMeshes(); m++)
			{
				if (eg::StringStartsWith(s_model->GetMesh(m).name, prefixSV))
				{
					s_doorMeshIndices[d][h] = m;
					found = true;
					break;
				}
			}
			EG_ASSERT(found);
		}
	}
}

EG_ON_INIT(InitEntranceEntity)

EntranceEntity::EntranceEntity()
{
	
}

glm::mat4 EntranceEntity::GetTransform() const
{
	glm::vec3 dir = DirectionVector(m_direction);
	const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
	
	const glm::vec3 translation = Position() - up * (MESH_HEIGHT / 2) + dir * MESH_LENGTH;
	
	if (m_type == Type::Exit)
		dir = -dir;
	const glm::mat3 rotation(dir, up, glm::cross(dir, up));
	
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation);
}

void EntranceEntity::Draw(ObjectRenderer& renderer)
{
	std::vector<glm::mat4> transforms(s_model->NumMeshes(), GetTransform());
	
	float doorMoveDist = glm::smoothstep(0.0f, 1.0f, m_doorOpenProgress) * 1.2f;
	for (int h = 0; h < 2; h++)
	{
		size_t meshIndex = s_doorMeshIndices[1 - (int)m_type][h];
		glm::vec3 tVec = glm::vec3(0, 1, -1) * doorMoveDist;
		if (h == 0)
			tVec = -tVec;
		transforms[meshIndex] = transforms[meshIndex] * glm::translate(glm::mat4(1.0f), tVec);
	}
	
	renderer.Add(*s_model, s_materials, transforms);
}

int EntranceEntity::GetEditorIconIndex() const
{
	return Entity::GetEditorIconIndex();
}

void EntranceEntity::EditorRenderSettings()
{
	Entity::EditorRenderSettings();
	ImGui::Combo("Type", reinterpret_cast<int*>(&m_type), "Entrance\0Exit\0");
}

void EntranceEntity::EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir)
{
	glm::vec3 position = newPosition;
	for (int i = 1; i <= 2; i++)
	{
		int idx = ((int)wallNormalDir / 2 + i) % 3;
		position[idx] = std::round(position[idx] - 0.5f) + 0.5f;
	}
	SetPosition(position);
	m_direction = OppositeDir(wallNormalDir);
}

void EntranceEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	glm::mat4 transform = GetTransform();
	
	if (m_type == Type::Exit)
	{
		transform *= glm::scale(glm::mat4(), glm::vec3(-1, 1, 1));
	}
	transform *= glm::translate(glm::mat4(), glm::vec3(-MESH_LENGTH - 0.01f, 0, 0));
	
	const eg::Model* model = m_type == Type::Entrance ? s_editorEntranceModel : s_editorExitModel;
	const ObjectMaterial* material = m_type == Type::Entrance ? s_editorEntranceMaterial : s_editorExitMaterial;
	
	drawArgs.objectRenderer->Add(*model, { &material, 1 }, { &transform, 1 });
}

void EntranceEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	emitter << YAML::Key << "dir" << YAML::Value << (int)m_direction;
	emitter << YAML::Key << "exit" << YAML::Value << (m_type == Type::Exit);
}

void EntranceEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	m_direction = (Dir)node["dir"].as<int>(0);
	m_type = node["exit"].as<bool>(true) ? Type::Exit : Type::Entrance;
}

void EntranceEntity::Update(const UpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	auto PlayerIsCloseTo = [&] (const glm::vec3& pos)
	{
		constexpr float MAX_DIST = 2.5f;
		return glm::distance2(args.player->Position(), pos) < MAX_DIST * MAX_DIST;
	};
	
	glm::vec3 dirToPlayer = args.player->Position() - Position();
	glm::vec3 desiredDirToPlayer = DirectionVector(m_direction) * (m_type == Type::Entrance ? 1 : -1);
	
	bool doorShouldBeOpen = PlayerIsCloseTo(Position()) && glm::dot(dirToPlayer, desiredDirToPlayer) > 0;
	
	if (((int)args.player->CurrentDown() / 2) == ((int)m_direction / 2))
		doorShouldBeOpen = false;
	
	if (!doorShouldBeOpen)
		m_timeBeforeClose = std::max(m_timeBeforeClose - args.dt, 0.0f);
	else
		m_timeBeforeClose = 0.5f;
	
	constexpr float TRANSITION_TIME = 0.2f;
	const float progressDelta = args.dt / TRANSITION_TIME;
	if (m_timeBeforeClose > 0)
		m_doorOpenProgress = std::min(m_doorOpenProgress + progressDelta, 1.0f);
	else
		m_doorOpenProgress = std::max(m_doorOpenProgress - progressDelta, 0.0f);
}

void EntranceEntity::CalcClipping(ClippingArgs& args) const
{
	glm::mat4 transform = GetTransform();
	
	constexpr float COL_SIZE_X = MESH_LENGTH;
	constexpr float COL_SIZE_Y = 3.0f;
	constexpr float COL_SIZE_Z = 1.5f;
	
	glm::vec3 colMin(transform * glm::vec4(-COL_SIZE_X, 0, -COL_SIZE_Z, 1));
	glm::vec3 colMax(transform * glm::vec4(COL_SIZE_X, COL_SIZE_Y, COL_SIZE_Z, 1));
	
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
			glm::vec3(colMin.x, colMin.y, colMax.z),
			glm::vec3(colMin.x, colMax.y, colMax.z),
			glm::vec3(colMin.x, colMax.y, colMin.z)
		},
		{ //Right side
			glm::vec3(colMax.x, colMin.y, colMin.z),
			glm::vec3(colMax.x, colMin.y, colMax.z),
			glm::vec3(colMax.x, colMax.y, colMax.z),
			glm::vec3(colMax.x, colMax.y, colMin.z)
		}
	};
	
	for (int i = 0; i < 4; i++)
	{
		CalcPolygonClipping(args, colPolygons[i]);
	}
	
	float doorsX[4] = { 4.7f, 4.3f, -4.3f, -4.7f };
	for (int i = 0; i < 4; i++)
	{
		if (m_doorOpenProgress > 0.1f && (i / 2) == (1 - (int)m_type))
			continue;
		
		glm::vec3 doorMin(transform * glm::vec4(doorsX[i], 0, -COL_SIZE_Z, 1));
		glm::vec3 doorMax(transform * glm::vec4(doorsX[i], COL_SIZE_Y, COL_SIZE_Z, 1));
		
		glm::vec3 doorPolygon[4] =
		{
			glm::vec3(doorMin.x, doorMin.y, doorMin.z),
			glm::vec3(doorMax.x, doorMin.y, doorMin.z),
			glm::vec3(doorMax.x, doorMax.y, doorMin.z),
			glm::vec3(doorMin.x, doorMax.y, doorMin.z)
		};
		CalcPolygonClipping(args, doorPolygon);
	}
}

void EntranceEntity::InitPlayer(Player& player) const
{
	player.SetPosition(Position() + glm::vec3(DirectionVector(m_direction)) * MESH_LENGTH);
	
	float rotations[6][2] =
	{
		{ eg::PI * 0.5f, 0 },
		{ eg::PI * 1.5f, 0 },
		{ 0, eg::HALF_PI },
		{ 0, -eg::HALF_PI },
		{ eg::PI * 2.0f, 0 },
		{ eg::PI * 1.0f, 0 }
	};
	player.SetRotation(rotations[(int)m_direction][0], rotations[(int)m_direction][1]);
}
