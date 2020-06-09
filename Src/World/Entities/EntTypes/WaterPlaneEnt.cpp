#include "WaterPlaneEnt.hpp"
#include "../../../../Protobuf/Build/WaterPlaneEntity.pb.h"

WaterPlaneEnt::WaterPlaneEnt()
{
	liquidPlane.SetShouldGenerateMesh(true);
	liquidPlane.SetEditorColor(eg::ColorSRGB::FromRGBAHex(0x67BEEA98));
}

const void* WaterPlaneEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(LiquidPlaneComp))
		return &liquidPlane;
	return Ent::GetComponent(type);
}

void WaterPlaneEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WaterPlaneEntity waterPlanePB;
	SerializePos(waterPlanePB, liquidPlane.position);
	waterPlanePB.set_wall_dir((gravity_pb::Dir)liquidPlane.wallForward);
	waterPlanePB.SerializeToOstream(&stream);
}

void WaterPlaneEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WaterPlaneEntity waterPlanePB;
	waterPlanePB.ParseFromIstream(&stream);
	liquidPlane.position = DeserializePos(waterPlanePB);
	liquidPlane.wallForward = (Dir)waterPlanePB.wall_dir();
}

void WaterPlaneEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	liquidPlane.position = newPosition;
	if (faceDirection)
		liquidPlane.wallForward = *faceDirection;
}

glm::vec3 WaterPlaneEnt::GetPosition() const
{
	return liquidPlane.position;
}
