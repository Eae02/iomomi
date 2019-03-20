#include "GravitySwitchEntity.hpp"
#include "../Voxel.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"

static eg::Model* s_model;
static eg::IMaterial* s_material;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/GravitySwitch.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
}

EG_ON_INIT(OnInit)

glm::mat4 GravitySwitchEntity::GetTransform() const
{
	glm::vec3 l = voxel::tangents[(int)m_up];
	glm::vec3 u = DirectionVector(m_up);
	return glm::translate(glm::mat4(1), Position()) * glm::mat4(glm::mat3(l, u, glm::cross(l, u)));
}

void GravitySwitchEntity::Draw(eg::MeshBatch& meshBatch)
{
	meshBatch.Add(*s_model, *s_material, GetTransform());
}

void GravitySwitchEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	drawArgs.meshBatch->Add(*s_model, *s_material, GetTransform());
}

void GravitySwitchEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	m_up = wallNormal;
	SetPosition(wallPosition);
}

void GravitySwitchEntity::EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir)
{
	m_up = wallNormalDir;
	SetPosition(newPosition);
}

int GravitySwitchEntity::GetEditorIconIndex() const
{
	return Entity::GetEditorIconIndex();
}

void GravitySwitchEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	emitter << YAML::Key << "up" << YAML::Value << (int)m_up;
}

void GravitySwitchEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	m_up = (Dir)node["up"].as<int>(0);
}
