#include "Entity.hpp"

#include <magic_enum/magic_enum.hpp>
#include <pcg_random.hpp>

#include "../../ImGui.hpp"
#include "Components/ActivatableComp.hpp"

static_assert(magic_enum::enum_count<EntTypeID>() == NUM_ENTITY_TYPES);

const std::array<EntTypeID, 14> entityUpdateOrder = {
	EntTypeID::ActivationLightStrip,
	EntTypeID::FloorButton,
	EntTypeID::PushButton,
	EntTypeID::ForceField,
	EntTypeID::GravityBarrier,
	EntTypeID::CubeSpawner,
	EntTypeID::Cube,
	EntTypeID::Platform,
	EntTypeID::GravitySwitch,
	EntTypeID::SlidingWall,
	EntTypeID::EntranceExit,
	EntTypeID::Gate,
	EntTypeID::WaterWall,
	EntTypeID::Pump,
};

std::array<std::optional<EntType>, NUM_ENTITY_TYPES> Ent::s_entityTypes;

void Ent::DefineTypeImpl(EntTypeID typeID, EntType type)
{
	size_t index = static_cast<size_t>(typeID);
	if (index >= NUM_ENTITY_TYPES)
	{
		EG_PANIC("Entity " << type.name << " using out of range id!");
	}
	if (s_entityTypes[index].has_value())
	{
		EG_PANIC("Entity " << type.name << " using occupied type id!");
	}
	s_entityTypes[index] = std::move(type);
}

std::string_view Ent::RemoveEntSuffix(std::string_view typeName)
{
	if (typeName.ends_with("Ent"))
		return typeName.substr(0, typeName.size() - 3);
	return typeName;
}

std::string Ent::TypeNameToPrettyName(std::string_view typeName)
{
	std::string prettyName;
	for (char c : RemoveEntSuffix(typeName))
	{
		if (std::isupper(c))
			prettyName.push_back(' ');
		prettyName.push_back(c);
	}
	return prettyName;
}

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

void Ent::CommonDraw(const EntDrawArgs& args) {}
void Ent::GameDraw(const EntGameDrawArgs& args)
{
	CommonDraw(args);
}
void Ent::EditorDraw(const EntEditorDrawArgs& args)
{
	CommonDraw(args);
}

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
		GetPosition() - diag, GetPosition() + diag + glm::vec3(DirectionVector(facingDirection)) * (upDist * scale)
	);
}

EntTypeFlags Ent::TypeFlags() const
{
	return s_entityTypes.at(static_cast<size_t>(m_typeID))->flags;
}

void Ent::Update(const struct WorldUpdateArgs& args) {}

void Ent::Spawned(bool isEditor) {}

std::shared_ptr<Ent> Ent::Clone() const
{
	std::shared_ptr<Ent> clone = s_entityTypes.at(static_cast<size_t>(m_typeID))->clone(*this);
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

const EntType* Ent::GetTypeByID(EntTypeID typeID)
{
	size_t idx = static_cast<size_t>(typeID);
	if (!s_entityTypes.at(idx).has_value())
		return nullptr;
	return &*s_entityTypes[idx];
}
