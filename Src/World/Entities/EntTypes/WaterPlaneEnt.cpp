#include "WaterPlaneEnt.hpp"
#include "../../../../Protobuf/Build/WaterPlaneEntity.pb.h"

#include <imgui.h>

WaterPlaneEnt::WaterPlaneEnt()
{
	liquidPlane.shouldGenerateMesh = true;
	liquidPlane.editorColor = eg::ColorSRGB::FromRGBAHex(0x67BEEA98);
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
	waterPlanePB.set_density_boost(densityBoost);
	waterPlanePB.set_wall_dir((gravity_pb::Dir)liquidPlane.wallForward);
	waterPlanePB.SerializeToOstream(&stream);
}

void WaterPlaneEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WaterPlaneEntity waterPlanePB;
	waterPlanePB.ParseFromIstream(&stream);
	densityBoost = waterPlanePB.density_boost();
	liquidPlane.position = DeserializePos(waterPlanePB);
	liquidPlane.wallForward = (Dir)waterPlanePB.wall_dir();
}

void WaterPlaneEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	liquidPlane.position = newPosition;
	if (faceDirection)
		liquidPlane.wallForward = *faceDirection;
	liquidPlane.MarkOutOfDate();
}

glm::vec3 WaterPlaneEnt::GetPosition() const
{
	return liquidPlane.position;
}

void WaterPlaneEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::DragInt("Density Boost", &densityBoost))
		densityBoost = std::max(densityBoost, 0);
}

int WaterPlaneEnt::GetEditorIconIndex() const
{
	return 16;
}
