#include "Entity.hpp"

#include <imgui.h>

bool Entity::EditorInteract(const Entity::EditorInteractArgs& args)
{
	return false;
}

void Entity::Save(YAML::Emitter& emitter) const
{
	emitter << YAML::Key << "pos" << YAML::Value << YAML::BeginSeq
	        << m_position[0] << m_position[1] << m_position[2] << YAML::EndSeq;
}

void Entity::Load(const YAML::Node& node)
{
	if (const YAML::Node& posNode = node["pos"])
	{
		for (int i = 0; i < 3; i++)
		{
			m_position[i] = posNode[i].as<float>();
		}
	}
}

void Entity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	m_position = wallPosition;
}

int Entity::GetEditorIconIndex() const
{
	return 5;
}

void Entity::EditorRenderSettings()
{
	ImGui::DragFloat3("Position", &m_position.x, 0.1f, -INFINITY, INFINITY, "%.1f");
}

void Entity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	
}

std::vector<EntityType> entityTypes;

void DefineEntityType(EntityType entityType)
{
	auto it = std::lower_bound(entityTypes.begin(), entityTypes.end(), entityType.Name(), [&] (const EntityType& a, const std::string& b)
	{
		return a.Name() < b;
	});
	
	entityTypes.insert(it, std::move(entityType));
}

const EntityType* FindEntityType(std::string_view name)
{
	auto it = std::lower_bound(entityTypes.begin(), entityTypes.end(), name, [&] (const EntityType& a, std::string_view b)
	{
		return a.Name() < b;
	});
	return (it != entityTypes.end() && it->Name() == name) ? &*it : nullptr;
}
