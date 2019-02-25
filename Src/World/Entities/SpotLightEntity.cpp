#include "SpotLightEntity.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"
#include <imgui.h>

static std::atomic<uint64_t> nextInstanceID(0);

SpotLightEntity::SpotLightEntity(const eg::ColorSRGB& color, float intensity,
                                 float yaw, float pitch, float cutoffAngle, float penumbraAngle)
	: m_instanceID(nextInstanceID.fetch_add(1))
{
	SetDirection(yaw, pitch);
	SetCutoff(cutoffAngle, penumbraAngle);
	SetRadiance(color, intensity);
}

void SpotLightEntity::SetDirection(float yaw, float pitch)
{
	float cosPitch = std::cos(pitch);
	glm::vec3 direction(
		std::cos(yaw) * cosPitch,
		std::sin(pitch),
		std::sin(yaw) * cosPitch
	);
	glm::vec3 directionL = glm::cross(direction, glm::vec3(0, 1, 0));
	
	if (glm::length2(directionL) < 1E-4f)
		directionL = glm::vec3(1, 0, 0);
	else
		directionL = glm::normalize(directionL);
	
	const glm::vec3 lightDirU = glm::cross(direction, directionL);
	m_rotationMatrix = glm::mat3(directionL, direction, lightDirU);
	
	m_yaw = yaw;
	m_pitch = pitch;
}

void SpotLightEntity::SetCutoff(float cutoffAngle, float penumbraAngle)
{
	if (cutoffAngle > eg::PI)
		cutoffAngle = eg::PI;
	if (penumbraAngle > cutoffAngle)
		penumbraAngle = cutoffAngle;
	
	float cosMinPAngle = std::cos(cutoffAngle - penumbraAngle);
	float cosMaxPAngle = std::cos(cutoffAngle);
	
	m_penumbraBias = -cosMaxPAngle;
	m_penumbraScale = 1.0f / (cosMinPAngle - cosMaxPAngle);
	
	m_cutoffAngle = cutoffAngle;
	m_penumbraAngle = penumbraAngle;
	
	m_width = std::tan(cutoffAngle);
}

void SpotLightEntity::SetRadiance(const eg::ColorSRGB& color, float intensity)
{
	m_radiance.r = eg::SRGBToLinear(color.r) * intensity;
	m_radiance.g = eg::SRGBToLinear(color.g) * intensity;
	m_radiance.b = eg::SRGBToLinear(color.b) * intensity;
	
	const float maxChannel = std::max(m_radiance.r, std::max(m_radiance.g, m_radiance.b));
	m_range = std::sqrt(256 * maxChannel + 1.0f);
	
	m_color = color;
	m_intensity = intensity;
}

void SpotLightEntity::InitDrawData(SpotLightDrawData& data) const
{
	data.position = Position();
	data.range = m_range;
	data.width = m_width;
	data.direction = GetDirection();
	data.directionL = GetDirectionL();
	data.penumbraBias = m_penumbraBias;
	data.penumbraScale = m_penumbraScale;
	data.radiance = m_radiance;
}

void SpotLightEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	SetPosition(wallPosition);
}

bool SpotLightEntity::EditorInteract(const Entity::EditorInteractArgs& args)
{
	return Entity::EditorInteract(args);
}

void SpotLightEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	
	emitter << YAML::Key << "range" << YAML::Value << m_range
	        << YAML::Key << "cutoff" << YAML::Value << m_cutoffAngle
	        << YAML::Key << "penumbra" << YAML::Value << m_penumbraAngle
	        << YAML::Key << "yaw" << YAML::Value << m_yaw
	        << YAML::Key << "pitch" << YAML::Value << m_pitch
	        << YAML::Key << "intensity" << YAML::Value << m_intensity
	        << YAML::Key << "color" << YAML::Value << YAML::BeginSeq
	        << m_color.r << m_color.g << m_color.b << YAML::EndSeq;
}

void SpotLightEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	
	SetCutoff(node["cutoff"].as<float>(eg::PI / 4), node["penumbra"].as<float>(eg::PI / 16));
	
	SetDirection(node["yaw"].as<float>(0), node["pitch"].as<float>(0));
	
	eg::ColorSRGB color(1, 1, 1);
	if (const YAML::Node& colorNode = node["color"])
	{
		color.r = colorNode[0].as<float>(1);
		color.g = colorNode[1].as<float>(1);
		color.b = colorNode[2].as<float>(1);
	}
	
	SetRadiance(color, node["intensity"].as<float>(1));
}

void SpotLightEntity::EditorRenderSettings()
{
	Entity::EditorRenderSettings();
	
	if (ImGui::SliderAngle("Yaw", &m_yaw))
	{
		SetDirection(m_yaw, m_pitch);
	}
	
	if (ImGui::SliderAngle("Pitch", &m_pitch, -90, 90))
	{
		SetDirection(m_yaw, m_pitch);
	}
	
	ImGui::Separator();
	
	if (ImGui::ColorEdit3("Color", &m_color.r))
	{
		SetRadiance(m_color, m_intensity);
	}
	
	if (ImGui::DragFloat("Intensity", &m_intensity, 0.1f, 0.0f, INFINITY, "%.1f"))
	{
		SetRadiance(m_color, m_intensity);
	}
	
	ImGui::Separator();
	
	if (ImGui::SliderAngle("Cutoff Angle", &m_cutoffAngle, 5.0f, 179.0f))
	{
		SetCutoff(m_cutoffAngle, m_penumbraAngle);
	}
	
	if (ImGui::SliderAngle("Penumbra Angle", &m_penumbraAngle, 0.0f, glm::degrees(m_cutoffAngle)))
	{
		SetCutoff(m_cutoffAngle, m_penumbraAngle);
	}
}

const glm::vec3 coneVertices[] = 
{
	glm::vec3(1, 1, 0),
	glm::vec3(-1, 1, 0),
	glm::vec3(0, 1, 1),
	glm::vec3(0, 1, -1)
};

void SpotLightEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	if (!selected)
		return;
	
	glm::vec3 forward = GetDirection();
	for (const glm::vec3& v : coneVertices)
	{
		float LINE_WIDTH = 0.01f;
		glm::vec3 dl = glm::cross(v, forward) * LINE_WIDTH;
		glm::vec3 positions[] = { dl, -dl, v + dl, v - dl };
		for (glm::vec3& pos : positions)
		{
			pos = Position() + m_rotationMatrix * (glm::vec3(m_width, 1.0f, m_width) * pos);
		}
		drawArgs.primitiveRenderer->AddQuad(positions, m_color);
	}
}
