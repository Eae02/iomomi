#include "Entity.hpp"
#include <imgui.h>

std::unordered_map<EntTypeID, EntType> entTypeMap;

static std::mt19937 nameGen;

Ent::Ent()
	: m_name(static_cast<uint32_t>(nameGen())) { }

void Ent::RenderSettings()
{
	ImGui::DragFloat3("Position", &m_position.x, 0.1f);
}

void Ent::Draw(const EntDrawArgs& args) { }
void Ent::EditorDraw(const EntEditorDrawArgs& args) { }

const void* Ent::GetComponent(const std::type_info& type) const
{
	return nullptr;
}

void Ent::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	m_direction = faceDirection.value_or(m_direction);
}

glm::mat4 Ent::GetTransform(float scale) const
{
	return glm::translate(glm::mat4(1), m_position) *
	       glm::mat4(GetRotationMatrix()) *
	       glm::scale(glm::mat4(1), glm::vec3(scale));
}

glm::mat3 Ent::GetRotationMatrix() const
{
	const glm::vec3 l = voxel::tangents[(int)m_direction];
	const glm::vec3 u = DirectionVector(m_direction);
	return glm::mat3(l, u, glm::cross(l, u));
}

eg::AABB Ent::GetAABB(float scale, float upDist) const
{
	glm::vec3 diag(scale);
	diag[(int)m_direction / 2] = 0;
	
	return eg::AABB(m_position - diag, m_position + diag + glm::vec3(DirectionVector(m_direction)) * (upDist * scale));
}

EntTypeFlags Ent::TypeFlags() const
{
	return entTypeMap.at(m_typeID).flags;
}
