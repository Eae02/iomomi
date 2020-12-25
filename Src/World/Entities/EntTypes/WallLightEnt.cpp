#include "WallLightEnt.hpp"
#include "../../World.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../../Protobuf/Build/WallLightEntity.pb.h"

#include <imgui.h>

static constexpr float LIGHT_DIST = 0.5f;
static constexpr float MODEL_SCALE = 0.25f;

void WallLightEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::Separator();
	
	ImGui::ColorPicker3("Color", &m_color.r);
	ImGui::DragFloat("Intensity", &m_intensity, 0.1f);
	
	if (ImGui::Button("Set Color Light Blue"))
		m_color = eg::ColorSRGB::FromHex(0xD1F8FE);
	if (ImGui::Button("Set Color Orange"))
		m_color = eg::ColorSRGB::FromHex(0xEED8BA);
	if (ImGui::Button("Set Color Green"))
		m_color = eg::ColorSRGB::FromHex(0x68E427);
}

EmissiveMaterial::InstanceData WallLightEnt::GetInstanceData(float colorScale) const
{
	eg::ColorLin color = eg::ColorLin(m_color).ScaleRGB(colorScale);
	
	EmissiveMaterial::InstanceData instanceData;
	instanceData.transform =
	       glm::translate(glm::mat4(1), m_position) *
	       glm::mat4(GetRotationMatrix(m_forwardDir)) *
	       glm::scale(glm::mat4(1), glm::vec3(MODEL_SCALE));
	
	instanceData.color = glm::vec4(color.r, color.g, color.b, 1.0f);
	
	return instanceData;
}

static eg::Model& GetModel()
{
	static eg::Model* model = nullptr;
	if (model == nullptr)
	{
		model = &eg::GetAsset<eg::Model>("Models/WallLight.obj");
	}
	return *model;
}

void WallLightEnt::CollectPointLights(std::vector<std::shared_ptr<PointLight>>& lights)
{
	glm::vec3 lightPos = m_position + glm::vec3(DirectionVector(m_forwardDir)) * LIGHT_DIST;
	lights.push_back(std::make_shared<PointLight>(lightPos, m_color, m_intensity));
}

void WallLightEnt::GameDraw(const EntGameDrawArgs& args)
{
	DrawEmissiveModel(args, 4.0f);
}

void WallLightEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	DrawEmissiveModel(args, 1.0f);
}

void WallLightEnt::DrawEmissiveModel(const EntDrawArgs& drawArgs, float colorScale) const
{
	drawArgs.meshBatch->AddModel(GetModel(), EmissiveMaterial::instance, GetInstanceData(colorScale));
}

void WallLightEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WallLightEntity wallLightPB;
	
	wallLightPB.set_dir((gravity_pb::Dir)m_forwardDir);
	SerializePos(wallLightPB, m_position);
	
	wallLightPB.set_colorr(m_color.r);
	wallLightPB.set_colorg(m_color.g);
	wallLightPB.set_colorb(m_color.b);
	wallLightPB.set_intensity(m_intensity);
	
	wallLightPB.SerializeToOstream(&stream);
}

void WallLightEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WallLightEntity wallLightPB;
	wallLightPB.ParseFromIstream(&stream);
	
	m_forwardDir = (Dir)wallLightPB.dir();
	m_position = DeserializePos(wallLightPB);
	
	m_color = eg::ColorSRGB(wallLightPB.colorr(), wallLightPB.colorg(), wallLightPB.colorb());
	m_intensity = wallLightPB.intensity();
}

void WallLightEnt::HalfLightIntensity()
{
	m_intensity /= 2;
}

void WallLightEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
	{
		m_forwardDir = *faceDirection;
	}
}
