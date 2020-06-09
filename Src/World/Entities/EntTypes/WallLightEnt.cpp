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
	
	m_pointLight.RenderRadianceSettings();
	
	if (ImGui::Button("Set Color Light Blue"))
		m_pointLight.SetRadiance(eg::ColorSRGB::FromHex(0xD1F8FE), m_pointLight.Intensity());
	if (ImGui::Button("Set Color Orange"))
		m_pointLight.SetRadiance(eg::ColorSRGB::FromHex(0xEED8BA), m_pointLight.Intensity());
	if (ImGui::Button("Set Color Green"))
		m_pointLight.SetRadiance(eg::ColorSRGB::FromHex(0x68E427), m_pointLight.Intensity());
}

EmissiveMaterial::InstanceData WallLightEnt::GetInstanceData(float colorScale) const
{
	eg::ColorLin color = eg::ColorLin(m_pointLight.GetColor()).ScaleRGB(colorScale);
	
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

void WallLightEnt::GameDraw(const EntGameDrawArgs& args)
{
	glm::vec3 lightPos = m_position + glm::vec3(DirectionVector(m_forwardDir)) * LIGHT_DIST;
	args.pointLights->emplace_back(m_pointLight.GetDrawData(lightPos));
	
	args.meshBatch->AddModel(GetModel(), EmissiveMaterial::instance, GetInstanceData(4.0f));
}

void WallLightEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	args.meshBatch->AddModel(GetModel(), EmissiveMaterial::instance, GetInstanceData(1.0f));
}

void WallLightEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WallLightEntity wallLightPB;
	
	wallLightPB.set_dir((gravity_pb::Dir)m_forwardDir);
	SerializePos(wallLightPB, m_position);
	
	wallLightPB.set_colorr(m_pointLight.GetColor().r);
	wallLightPB.set_colorg(m_pointLight.GetColor().g);
	wallLightPB.set_colorb(m_pointLight.GetColor().b);
	wallLightPB.set_intensity(m_pointLight.Intensity());
	
	wallLightPB.SerializeToOstream(&stream);
}

void WallLightEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WallLightEntity wallLightPB;
	wallLightPB.ParseFromIstream(&stream);
	
	m_forwardDir = (Dir)wallLightPB.dir();
	m_position = DeserializePos(wallLightPB);
	
	eg::ColorSRGB color(wallLightPB.colorr(), wallLightPB.colorg(), wallLightPB.colorb());
	m_pointLight.SetRadiance(color, wallLightPB.intensity());
}

void WallLightEnt::HalfLightIntensity()
{
	m_pointLight.SetRadiance(m_pointLight.GetColor(), m_pointLight.Intensity() / 2);
}

void WallLightEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_forwardDir = *faceDirection;
}
