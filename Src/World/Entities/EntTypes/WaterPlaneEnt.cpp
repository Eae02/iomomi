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

void WaterPlaneEnt::Serialize(std::ostream& stream)
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
