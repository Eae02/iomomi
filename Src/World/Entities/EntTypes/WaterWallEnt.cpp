#include "WaterWallEnt.hpp"

#include "../../../ImGui.hpp"
#include "../../WorldUpdateArgs.hpp"
#include <WaterWallEntity.pb.h>

DEF_ENT_TYPE(WaterWallEnt)

WaterWallEnt::WaterWallEnt()
{
	m_waterBlockComp.onlyBlockDuringSimulation = true;
}

void WaterWallEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	bool geometryChanged = false;

	geometryChanged |= ImGui::DragFloat2("Size", &m_aaQuad.radius.x, 0.5f);

	geometryChanged |= ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");

	ImGui::Checkbox("Only Initially", &m_onlyInitially);

	if (geometryChanged)
	{
		m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
		m_waterBlockComp.editorVersion++;
	}
#endif
}

void WaterWallEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	auto [tangent, biTangent] = m_aaQuad.GetTangents(0);
	tangent *= 0.5f;
	biTangent *= 0.5f;

	const glm::vec3 vertices[] = {
		m_position - tangent - biTangent,
		m_position - tangent + biTangent,
		m_position + tangent - biTangent,
		m_position + tangent + biTangent,
	};

	eg::ColorSRGB color = eg::ColorSRGB::FromRGBAHex(0x7034DA99);
	color = color.ScaleAlpha(args.getDrawMode(this) == EntEditorDrawMode::Selected ? 0.5f : 0.1f);

	args.primitiveRenderer->AddLine(vertices[0], vertices[1], color);
	args.primitiveRenderer->AddLine(vertices[0], vertices[2], color);
	args.primitiveRenderer->AddLine(vertices[3], vertices[1], color);
	args.primitiveRenderer->AddLine(vertices[3], vertices[2], color);
	args.primitiveRenderer->AddQuad(vertices, color);
}

void WaterWallEnt::Update(const WorldUpdateArgs& args)
{
	if (args.mode == WorldMode::Game && m_timeUntilDisable > 0)
	{
		m_timeUntilDisable -= args.dt;
		if (m_timeUntilDisable <= 0)
		{
			m_waterBlockComp.blockedGravities = DirFlags::None;
		}
	}
}

const void* WaterWallEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	if (type == typeid(WaterBlockComp))
		return &m_waterBlockComp;
	return nullptr;
}

void WaterWallEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
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
	iomomi_pb::WaterWallEntity waterWallPB;

	SerializePos(waterWallPB, m_position);

	waterWallPB.set_up_plane(m_aaQuad.upPlane);
	waterWallPB.set_sizex(m_aaQuad.radius.x);
	waterWallPB.set_sizey(m_aaQuad.radius.y);
	waterWallPB.set_only_initially(m_onlyInitially);

	waterWallPB.SerializeToOstream(&stream);
}

void WaterWallEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::WaterWallEntity waterWallPB;
	waterWallPB.ParseFromIstream(&stream);

	m_position = DeserializePos(waterWallPB);

	m_aaQuad.upPlane = waterWallPB.up_plane();
	m_aaQuad.radius = glm::vec2(waterWallPB.sizex(), waterWallPB.sizey());
	m_onlyInitially = waterWallPB.only_initially();
	if (m_onlyInitially)
	{
		m_timeUntilDisable = 2;
	}

	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
}

int WaterWallEnt::EdGetIconIndex() const
{
	return 18;
}
