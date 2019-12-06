#include "WaterPlaneEnt.hpp"
#include "../../../../Protobuf/Build/WaterPlaneEntity.pb.h"

WaterPlaneEnt::WaterPlaneEnt()
{
	m_liquidPlane.SetShouldGenerateMesh(true);
	m_liquidPlane.SetEditorColor(eg::ColorSRGB::FromRGBAHex(0x67BEEA98));
}

const void* WaterPlaneEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(LiquidPlaneComp))
		return &m_liquidPlane;
	return Ent::GetComponent(type);
}

void WaterPlaneEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WaterPlaneEntity waterPlanePB;
	SerializePos(waterPlanePB);
	waterPlanePB.SerializeToOstream(&stream);
}

void WaterPlaneEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WaterPlaneEntity waterPlanePB;
	waterPlanePB.ParseFromIstream(&stream);
	DeserializePos(waterPlanePB);
}

template <>
std::shared_ptr<Ent> CloneEntity<WaterPlaneEnt>(const Ent& entity)
{
	std::shared_ptr<WaterPlaneEnt> newEntity = Ent::Create<WaterPlaneEnt>();
	newEntity->m_position = entity.Pos();
	newEntity->m_direction = entity.Direction();
	return newEntity;
}
