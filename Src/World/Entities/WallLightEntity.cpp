#include "WallLightEntity.hpp"
#include "../Voxel.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"

static eg::Model* s_model;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/WallLight.obj");
}

EG_ON_INIT(OnInit)

static constexpr float LIGHT_DIST = 0.5f;

glm::mat4 WallLightEntity::GetTransform() const
{
	glm::vec3 l = voxel::tangents[(int)m_dir];
	glm::vec3 u = DirectionVector(m_dir);
	return glm::translate(glm::mat4(1), Position()) *
		glm::mat4(glm::mat3(l, u, glm::cross(l, u))) *
		glm::scale(glm::mat4(1), glm::vec3(0.25f));
}

void WallLightEntity::Draw(eg::MeshBatch& meshBatch)
{
	m_material.SetColor(eg::ColorLin(GetColor()).ScaleRGB(4.0f));
	meshBatch.Add(*s_model, m_material, GetTransform());
}

void WallLightEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	drawArgs.meshBatch->Add(*s_model, m_material, GetTransform());
}

void WallLightEntity::EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir)
{
	SetPosition(newPosition);
	m_dir = wallNormalDir;
}

void WallLightEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	SetPosition(wallPosition);
	m_dir = wallNormal;
}

void WallLightEntity::Save(YAML::Emitter& emitter) const
{
	emitter << YAML::Key << "up" << YAML::Value << (int)m_dir;
	PointLightEntity::Save(emitter);
}

void WallLightEntity::Load(const YAML::Node& node)
{
	m_dir = (Dir)node["up"].as<int>(0);
	PointLightEntity::Load(node);
}

void WallLightEntity::InitDrawData(PointLightDrawData& data) const
{
	PointLightEntity::InitDrawData(data);
	data.pc.position += LIGHT_DIST * glm::vec3(DirectionVector(m_dir));
}
