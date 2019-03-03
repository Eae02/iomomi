#include "EntranceEntity.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Graphics/ObjectRenderer.hpp"

constexpr float MESH_LENGTH = 3.2f;

EntranceEntity::EntranceEntity()
{
	m_model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	m_materials.resize(m_model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	m_materials.at(m_model->GetMaterialIndex("Floor")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Floor.yaml");
	m_materials.at(m_model->GetMaterialIndex("WallPadding")) = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance/Padding.yaml");
}

glm::mat4 EntranceEntity::GetTransform() const
{
	glm::vec3 dir = DirectionVector(m_direction);
	glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
	
	glm::mat3 rotation(dir, up, glm::cross(dir, up));
	
	return glm::translate(glm::mat4(1.0f), Position() + dir * MESH_LENGTH) * glm::mat4(rotation);
}

void EntranceEntity::Draw(ObjectRenderer& renderer)
{
	renderer.Add(*m_model, m_materials, GetTransform());
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
	
}

void EntranceEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	drawArgs.objectRenderer->Add(*m_model, m_materials, GetTransform());
}

void EntranceEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
}

void EntranceEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
}
