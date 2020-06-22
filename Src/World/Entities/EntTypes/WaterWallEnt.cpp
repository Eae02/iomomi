#include "WaterWallEnt.hpp"
#include "../../../Editor/PrimitiveRenderer.hpp"
#include "../../../../Protobuf/Build/WaterWallEntity.pb.h"
#include <imgui.h>

WaterWallEnt::WaterWallEnt()
{
	std::fill_n(m_waterBlockComp.blockedGravities, 6, true);
}

void WaterWallEnt::RenderSettings()
{
	bool geometryChanged = false;
	
	geometryChanged |= ImGui::DragFloat2("Size", &m_aaQuad.radius.x, 0.5f);
	
	geometryChanged |= ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");
	
	if (geometryChanged)
	{
		m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
		m_waterBlockComp.editorVersion++;
	}
}

void WaterWallEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	auto [tangent, biTangent] = m_aaQuad.GetTangents(0);
	tangent *= 0.5f;
	biTangent *= 0.5f;
	glm::vec3 vertices[] = 
	{
		m_position - tangent - biTangent,
		m_position - tangent + biTangent,
		m_position + tangent - biTangent,
		m_position + tangent + biTangent
	};
	
	args.primitiveRenderer->AddQuad(vertices, eg::ColorSRGB::FromRGBAHex(0x7034DA99));
}

const void* WaterWallEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	if (type == typeid(WaterBlockComp))
		return &m_waterBlockComp;
	return nullptr;
}

void WaterWallEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
	m_waterBlockComp.editorVersion++;
	m_position = newPosition;
}

glm::vec3 WaterWallEnt::GetPosition() const
{
	return m_position;
}

void WaterWallEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WaterWallEntity windowPB;
	
	SerializePos(windowPB, m_position);
	
	windowPB.set_up_plane(m_aaQuad.upPlane);
	windowPB.set_sizex(m_aaQuad.radius.x);
	windowPB.set_sizey(m_aaQuad.radius.y);
	
	windowPB.SerializeToOstream(&stream);
}

void WaterWallEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WaterWallEntity windowPB;
	windowPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(windowPB);
	
	m_aaQuad.upPlane = windowPB.up_plane();
	m_aaQuad.radius = glm::vec2(windowPB.sizex(), windowPB.sizey());
	
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
}
