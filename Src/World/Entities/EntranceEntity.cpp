#include "EntranceEntity.hpp"
#include "../Player.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Graphics/ObjectRenderer.hpp"

constexpr float MESH_LENGTH = 4.8f;
constexpr float MESH_HEIGHT = 3.0f;

EntranceEntity::EntranceEntity()
{
	m_model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	m_materials.resize(m_model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	m_materials.at(m_model->GetMaterialIndex("Floor")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Floor.yaml");
	m_materials.at(m_model->GetMaterialIndex("WallPadding")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Padding.yaml");
	
	for (int d = 0; d < 2; d++)
	{
		for (int h = 0; h < 2; h++)
		{
			char prefix[] = { 'D', 'o', 'o', 'r', d ? '2' : '1', h ? 'B' : 'A' };
			std::string_view prefixSV(prefix, eg::ArrayLen(prefix));
			
			bool found = false;
			for (size_t m = 0; m < m_model->NumMeshes(); m++)
			{
				if (eg::StringStartsWith(m_model->GetMesh(m).name, prefixSV))
				{
					m_doorMeshIndices[d][h] = m;
					found = true;
					break;
				}
			}
			EG_ASSERT(found);
		}
	}
}

glm::mat4 EntranceEntity::GetTransform() const
{
	const glm::vec3 dir = DirectionVector(m_direction);
	const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
	
	const glm::mat3 rotation(dir, up, glm::cross(dir, up));
	const glm::vec3 translation = Position() - up * (MESH_HEIGHT / 2) + dir * MESH_LENGTH;
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation);
}

void EntranceEntity::Draw(ObjectRenderer& renderer)
{
	std::vector<glm::mat4> transforms(m_model->NumMeshes(), GetTransform());
	
	float doorMoveDist = glm::smoothstep(0.0f, 1.0f, m_doorOpenProgress) * 1.2f;
	int doorSide = (int)m_type;
	for (int h = 0; h < 2; h++)
	{
		size_t meshIndex = m_doorMeshIndices[doorSide][h];
		glm::vec3 tVec = glm::vec3(0, 1, -1) * doorMoveDist;
		if (h == 0)
			tVec = -tVec;
		transforms[meshIndex] = transforms[meshIndex] * glm::translate(glm::mat4(1.0f), tVec);
	}
	
	renderer.Add(*m_model, m_materials, transforms);
}

int EntranceEntity::GetEditorIconIndex() const
{
	return Entity::GetEditorIconIndex();
}

void EntranceEntity::EditorRenderSettings()
{
	Entity::EditorRenderSettings();
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
	glm::mat4 transform[1] = { GetTransform() };
	drawArgs.objectRenderer->Add(*m_model, m_materials, transform);
}

void EntranceEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	emitter << YAML::Key << "dir" << YAML::Value << (int)m_direction;
}

void EntranceEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	m_direction = (Dir)node["dir"].as<int>(0);
}

void EntranceEntity::Update(const UpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	auto PlayerIsCloseTo = [&] (const glm::vec3& pos)
	{
		constexpr float MAX_DIST = 2.0f;
		return glm::distance2(args.player->Position(), pos) < MAX_DIST * MAX_DIST;
	};
	
	bool doorShouldBeOpen = false;
	if (m_type == Type::Exit)
	{
		doorShouldBeOpen = PlayerIsCloseTo(Position());
	}
	
	if (((int)args.player->CurrentDown() / 2) == ((int)m_direction / 2))
		doorShouldBeOpen = false;
	
	constexpr float TRANSITION_TIME = 0.2f;
	const float progressDelta = args.dt / TRANSITION_TIME;
	if (doorShouldBeOpen)
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
		},
	};
	
	//Expands polygons along x and y
	for (int i = 0; i < 4; i++)
	{
		glm::vec3 center;
		for (int j = 0; j < 4; j++)
			center += colPolygons[i][j];
		center /= 4.0f;
		for (int j = 0; j < 4; j++)
		{
			colPolygons[i][j] = (colPolygons[i][j] - center) * glm::vec3(1.2f, 1.2f, -1.0f) + center;
		}
	}
	
	for (int i = 0; i < 4; i++)
	{
		CalcPolygonClipping(args, colPolygons[i]);
	}
}
