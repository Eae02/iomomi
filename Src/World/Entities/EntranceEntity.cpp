#include "EntranceEntity.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Graphics/ObjectRenderer.hpp"

EntranceEntity::EntranceEntity()
{
	m_model = &eg::GetAsset<eg::Model>("Models/EnterRoom.obj");
	m_material = &eg::GetAsset<StaticPropMaterial>("Materials/Entrance.yaml");
}

glm::mat4 EntranceEntity::GetTransform() const
{
	return glm::translate(glm::mat4(1.0f), Position());
}

void EntranceEntity::Draw(ObjectRenderer& renderer)
{
	renderer.Add(*m_model, *m_material, GetTransform());
}

int EntranceEntity::GetEditorIconIndex() const
{
	return Entity::GetEditorIconIndex();
}

void EntranceEntity::EditorRenderSettings()
{
	Entity::EditorRenderSettings();
}

void EntranceEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	drawArgs.objectRenderer->Add(*m_model, *m_material, GetTransform());
}

void EntranceEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
}

void EntranceEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
}
