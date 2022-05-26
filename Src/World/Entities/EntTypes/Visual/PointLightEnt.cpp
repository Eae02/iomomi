#include "PointLightEnt.hpp"
#include "../../../World.hpp"
#include "../../../../ImGui.hpp"
#include "../../../../../Protobuf/Build/PointLightEntity.pb.h"

const eg::ColorSRGB PointLightEnt::DefaultColor = eg::ColorSRGB::FromHex(0xD1F8FE);
const float PointLightEnt::DefaultIntensity = 15;

void PointLightEnt::ColorAndIntensitySettings(eg::ColorSRGB& color, float& intensity, bool& enableSpecularHighlights)
{
#ifdef EG_HAS_IMGUI
	ImGui::ColorPicker3("Color", &color.r);
	ImGui::DragFloat("Intensity", &intensity, 0.1f);
	ImGui::Checkbox("Specular Highlights", &enableSpecularHighlights);
	
	ImGui::Separator();
	
	if (ImGui::Button("Set Color Light Blue"))
		color = eg::ColorSRGB::FromHex(0xD1F8FE);
	if (ImGui::Button("Set Color Orange"))
		color = eg::ColorSRGB::FromHex(0xEED8BA);
	if (ImGui::Button("Set Color Green"))
		color = eg::ColorSRGB::FromHex(0x68E427);
#endif
}

PointLightEnt::PointLightEnt()
	: m_color(DefaultColor), m_intensity(DefaultIntensity) { }

void PointLightEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::Separator();
	
	ColorAndIntensitySettings(m_color, m_intensity, m_enableSpecularHighlights);
}

void PointLightEnt::CollectPointLights(std::vector<std::shared_ptr<PointLight>>& lights)
{
	auto pointLight = std::make_shared<PointLight>(m_position, m_color, m_intensity);
	pointLight->enableSpecularHighlights = m_enableSpecularHighlights;
	pointLight->limitRangeByInitialPosition = true;
	lights.push_back(std::move(pointLight));
}

void PointLightEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
}

int PointLightEnt::EdGetIconIndex() const
{
	return 3;
}

void PointLightEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::PointLightEntity entityPB;
	
	SerializePos(entityPB, m_position);
	
	entityPB.set_colorr(m_color.r);
	entityPB.set_colorg(m_color.g);
	entityPB.set_colorb(m_color.b);
	entityPB.set_intensity(m_intensity);
	entityPB.set_no_specular(!m_enableSpecularHighlights);
	
	entityPB.SerializeToOstream(&stream);
}

void PointLightEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::PointLightEntity entityPB;
	entityPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(entityPB);
	m_color = eg::ColorSRGB(entityPB.colorr(), entityPB.colorg(), entityPB.colorb());
	m_intensity = entityPB.intensity();
	m_enableSpecularHighlights = !entityPB.no_specular();
}
