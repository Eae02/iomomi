#include "WallLightEnt.hpp"

#include "../../../../../Protobuf/Build/WallLightEntity.pb.h"
#include "../../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../../ImGui.hpp"
#include "../../../World.hpp"
#include "PointLightEnt.hpp"

static constexpr float LIGHT_DIST = 0.5f;
static constexpr float MODEL_SCALE = 0.25f;

DEF_ENT_TYPE(WallLightEnt)

WallLightEnt::WallLightEnt() : m_color(PointLightEnt::DefaultColor), m_intensity(PointLightEnt::DefaultIntensity) {}

void WallLightEnt::RenderSettings()
{
	Ent::RenderSettings();

	ImGui::Separator();

	PointLightEnt::ColorAndIntensitySettings(m_color, m_intensity, m_enableSpecularHighlights);
}

EmissiveMaterial::InstanceData WallLightEnt::GetInstanceData(float colorScale) const
{
	eg::ColorLin color = eg::ColorLin(m_color).ScaleRGB(colorScale);

	EmissiveMaterial::InstanceData instanceData;
	instanceData.transform = glm::translate(glm::mat4(1), m_position) * glm::mat4(GetRotationMatrix(m_forwardDir)) *
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
	auto pointLight = std::make_shared<PointLight>(lightPos, m_color, m_intensity);
	pointLight->enableSpecularHighlights = m_enableSpecularHighlights;
	pointLight->limitRangeByInitialPosition = true;
	lights.push_back(std::move(pointLight));
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

void WallLightEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
	{
		m_forwardDir = *faceDirection;
	}
}

int WallLightEnt::EdGetIconIndex() const
{
	return 3;
}

void WallLightEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::WallLightEntity entityPB;

	entityPB.set_dir(static_cast<iomomi_pb::Dir>(m_forwardDir));
	SerializePos(entityPB, m_position);

	entityPB.set_colorr(m_color.r);
	entityPB.set_colorg(m_color.g);
	entityPB.set_colorb(m_color.b);
	entityPB.set_intensity(m_intensity);
	entityPB.set_no_specular(!m_enableSpecularHighlights);

	entityPB.SerializeToOstream(&stream);
}

void WallLightEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::WallLightEntity entityPB;
	entityPB.ParseFromIstream(&stream);

	m_forwardDir = static_cast<Dir>(entityPB.dir());
	m_position = DeserializePos(entityPB);

	m_color = eg::ColorSRGB(entityPB.colorr(), entityPB.colorg(), entityPB.colorb());
	m_intensity = entityPB.intensity();
	m_enableSpecularHighlights = !entityPB.no_specular();
}
