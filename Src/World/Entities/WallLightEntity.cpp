#include "WallLightEntity.hpp"
#include "../Voxel.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"
#include "../../YAMLUtils.hpp"
#include "../../Graphics/Lighting/Serialize.hpp"

#include <imgui.h>

static eg::Model* s_model;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/WallLight.obj");
}

EG_ON_INIT(OnInit)

static constexpr float LIGHT_DIST = 0.5f;

glm::mat4 WallLightEntity::GetTransform() const
{
	glm::vec3 l = voxel::tangents[(int)m_dir];
	glm::vec3 u = DirectionVector(m_dir);
	return glm::translate(glm::mat4(1), Position()) *
		glm::mat4(glm::mat3(l, u, glm::cross(l, u))) *
		glm::scale(glm::mat4(1), glm::vec3(0.25f));
}

EmissiveMaterial::InstanceData WallLightEntity::GetInstanceData(float colorScale) const
{
	eg::ColorLin color = eg::ColorLin(m_pointLight.GetColor()).ScaleRGB(colorScale);
	
	EmissiveMaterial::InstanceData instanceData;
	instanceData.transform = GetTransform();
	instanceData.color = glm::vec4(color.r, color.g, color.b, 1.0f);
	
	return instanceData;
}

void WallLightEntity::Draw(eg::MeshBatch& meshBatch)
{
	meshBatch.Add(*s_model, EmissiveMaterial::instance, GetInstanceData(4.0f));
}

void WallLightEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	drawArgs.meshBatch->Add(*s_model, EmissiveMaterial::instance, GetInstanceData(1.0f));
}

void WallLightEntity::EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir)
{
	SetPosition(newPosition);
	m_dir = wallNormalDir;
}

void WallLightEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	SetPosition(wallPosition);
	m_dir = wallNormal;
}

void WallLightEntity::Save(YAML::Emitter& emitter) const
{
	emitter << YAML::Key << "up" << YAML::Value << (int)m_dir;
	SerializePointLight(emitter, m_pointLight);
	Entity::Save(emitter);
}

void WallLightEntity::Load(const YAML::Node& node)
{
	m_dir = (Dir)node["up"].as<int>(0);
	DeserializePointLight(node, m_pointLight);
	Entity::Load(node);
}

void WallLightEntity::GetPointLights(std::vector<PointLightDrawData>& drawData) const
{
	drawData.push_back(m_pointLight.GetDrawData(Position() + LIGHT_DIST * glm::vec3(DirectionVector(m_dir))));
}

void WallLightEntity::EditorRenderSettings()
{
	m_pointLight.RenderRadianceSettings();
}

int WallLightEntity::GetEditorIconIndex() const
{
	return 2;
}
