#include "ColliderEnt.hpp"
#include "Activation/CubeEnt.hpp"
#include "../../Player.hpp"
#include "../../../../Protobuf/Build/ColliderEntity.pb.h"

#include <imgui.h>

bool ColliderEnt::drawInEditor = true;

bool ColliderEnt::ShouldCollide(const PhysicsObject& self, const PhysicsObject& other)
{
	ColliderEnt* colldier = (ColliderEnt*)std::get<Ent*>(self.owner);
	std::optional<Dir> otherDown;
	
	if (std::holds_alternative<Player*>(other.owner))
	{
		if (!colldier->m_blockPlayer)
			return false;
		otherDown = std::get<Player*>(other.owner)->CurrentDown();
	}
	else if (std::holds_alternative<Ent*>(other.owner))
	{
		const Ent& otherEntity = *std::get<Ent*>(other.owner);
		if (otherEntity.TypeID() == EntTypeID::Cube)
		{
			if (!colldier->m_blockCubes)
				return false;
			otherDown = static_cast<const CubeEnt&>(otherEntity).CurrentDown();
		}
	}
	
	if (otherDown.has_value() && !colldier->m_blockedGravityModes[(int)*otherDown])
		return false;
	
	return true;
}

ColliderEnt::ColliderEnt()
{
	std::fill_n(m_blockedGravityModes, 6, true);
	
	m_physicsObject.rayIntersectMask = 0;
	m_physicsObject.canBePushed = false;
	m_physicsObject.debugColor = 0xc3236d;
	m_physicsObject.shouldCollide = &ColliderEnt::ShouldCollide;
	m_physicsObject.owner = this;
}

void ColliderEnt::RenderSettings()
{
	Ent::RenderSettings();
	ImGui::DragFloat3("Radius", &m_radius.x, 0.1f);
	ImGui::Separator();
	
	ImGui::TextDisabled("Block by Type");
	ImGui::Checkbox("Block Player", &m_blockPlayer);
	ImGui::Checkbox("Block Cubes", &m_blockCubes);
	ImGui::Checkbox("Block Picking Up", &m_blockPickUp);
	
	ImGui::Separator();
	
	ImGui::TextDisabled("Block by Gravity Mode");
	ImGui::Checkbox("Block +X", &m_blockedGravityModes[0]);
	ImGui::Checkbox("Block -X", &m_blockedGravityModes[1]);
	ImGui::Checkbox("Block +Y", &m_blockedGravityModes[2]);
	ImGui::Checkbox("Block -Y", &m_blockedGravityModes[3]);
	ImGui::Checkbox("Block +Z", &m_blockedGravityModes[4]);
	ImGui::Checkbox("Block -Z", &m_blockedGravityModes[5]);
}

std::optional<eg::ColorSRGB> ColliderEnt::EdGetBoxColor(bool selected) const
{
	if (!drawInEditor) return { };
	return eg::ColorSRGB::FromRGBAHex(0xc3236d33);
}

glm::vec3 ColliderEnt::EdGetSize() const
{
	return m_radius;
}

void ColliderEnt::EdResized(const glm::vec3& newSize)
{
	m_radius = newSize;
}

void ColliderEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_physicsObject.position = newPosition;
}

void ColliderEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void ColliderEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::ColliderEntity entityPB;
	
	SerializePos(entityPB, m_physicsObject.position);
	
	entityPB.set_radx(m_radius.x);
	entityPB.set_rady(m_radius.y);
	entityPB.set_radz(m_radius.z);
	
	uint32_t blockBitMask =
		((uint32_t)m_blockPlayer << 0U) |
		((uint32_t)m_blockCubes << 1U) |
		((uint32_t)m_blockPickUp << 8U);
	for (uint32_t i = 0; i < 6; i++)
	{
		blockBitMask |= (uint32_t)m_blockedGravityModes[i] << (2 + i);
	}
	entityPB.set_block_bit_mask(blockBitMask);
	
	entityPB.SerializeToOstream(&stream);
}

void ColliderEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::ColliderEntity entityPB;
	entityPB.ParseFromIstream(&stream);
	
	m_physicsObject.position = DeserializePos(entityPB);
	m_radius = glm::vec3(entityPB.radx(), entityPB.rady(), entityPB.radz());
	m_physicsObject.shape = eg::AABB(-m_radius, m_radius);
	
	m_blockPlayer = (entityPB.block_bit_mask() & 1U) != 0;
	m_blockCubes = (entityPB.block_bit_mask() & 2U) != 0;
	for (uint32_t i = 0; i < 6; i++)
	{
		m_blockedGravityModes[i] = (entityPB.block_bit_mask() & (1 << (i + 2))) != 0;
	}
	m_blockPickUp = (entityPB.block_bit_mask() & 256U) != 0;
	
	m_physicsObject.rayIntersectMask = m_blockPickUp ? RAY_MASK_BLOCK_PICK_UP : 0;
}
