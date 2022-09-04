#include "Entity.hpp"
#include "Components/ActivatableComp.hpp"
#include "../../ImGui.hpp"

#include <pcg_random.hpp>

std::optional<EntType> entityTypes[(size_t)EntTypeID::MAX];

extern pcg32_fast globalRNG;

uint32_t Ent::GenerateRandomName()
{
	return globalRNG();
}

void Ent::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	glm::vec3 position = GetPosition();
	if (ImGui::DragFloat3("Position", &position.x, 0.1f))
	{
		EdMoved(position, {});
	}
#endif
}

void Ent::CommonDraw(const EntDrawArgs& args) { }
void Ent::GameDraw(const EntGameDrawArgs& args) { CommonDraw(args); }
void Ent::EditorDraw(const EntEditorDrawArgs& args) { CommonDraw(args); }

const void* Ent::GetComponent(const std::type_info& type) const
{
	return nullptr;
}

glm::mat3 Ent::GetRotationMatrix(Dir dir)
{
	glm::vec3 l = voxel::tangents[static_cast<int>(dir)];
	glm::vec3 u = DirectionVector(dir);
	return glm::mat3(l, u, glm::cross(l, u));
}

eg::AABB Ent::GetAABB(float scale, float upDist, Dir facingDirection) const
{
	glm::vec3 diag(scale);
	diag[static_cast<int>(facingDirection) / 2] = 0;
	
	return eg::AABB(
		GetPosition() - diag,
		GetPosition() + diag + glm::vec3(DirectionVector(facingDirection)) * (upDist * scale)
	);
}

EntTypeFlags Ent::TypeFlags() const
{
	return entityTypes[static_cast<int>(m_typeID)]->flags;
}

void Ent::Update(const struct WorldUpdateArgs& args) { }

void Ent::Spawned(bool isEditor) { }

std::shared_ptr<Ent> Ent::Clone() const
{
	std::shared_ptr<Ent> clone = entityTypes[static_cast<int>(m_typeID)]->clone(*this);
	if (ActivatableComp* activatable = clone->GetComponentMut<ActivatableComp>())
		activatable->GiveNewName();
	return clone;
}

glm::vec3 Ent::EdGetGridAlignment() const
{
	return glm::vec3(0.1f);
}

int Ent::EdGetIconIndex() const
{
	return 5;
}

const EntType* GetEntityType(EntTypeID typeID)
{
	if (static_cast<int>(typeID) < 0 || static_cast<int>(typeID) >= static_cast<int>(EntTypeID::MAX))
		return nullptr;
	if (!entityTypes[static_cast<int>(typeID)].has_value())
		return nullptr;
	return &*entityTypes[static_cast<int>(typeID)];
}
