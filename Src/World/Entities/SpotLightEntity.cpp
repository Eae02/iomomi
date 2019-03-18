#include "SpotLightEntity.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"
#include <imgui.h>

SpotLightEntity::SpotLightEntity(const eg::ColorSRGB& color, float intensity,
                                 float cutoffAngle, float penumbraAngle)
	: LightSourceEntity(color, intensity)
{
	SetDirection(glm::vec3(0, 1, 0));
	SetCutoff(cutoffAngle, penumbraAngle);
}

void SpotLightEntity::SetDirection(const glm::vec3& direction)
{
	glm::vec3 directionN = glm::normalize(direction);
	glm::vec3 directionL = glm::cross(directionN, glm::vec3(0, 1, 0));
	
	if (glm::length2(directionL) < 1E-4f)
		directionL = glm::vec3(1, 0, 0);
	else
		directionL = glm::normalize(directionL);
	
	const glm::vec3 lightDirU = glm::cross(directionN, directionL);
	m_rotationMatrix = glm::mat3(directionL, directionN, lightDirU);
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

void SpotLightEntity::InitDrawData(SpotLightDrawData& data) const
{
	data.position = Position();
	data.range = Range();
	data.width = m_width;
	data.direction = GetDirection();
	data.directionL = GetDirectionL();
	data.penumbraBias = m_penumbraBias;
	data.penumbraScale = m_penumbraScale;
	data.radiance = Radiance();
}

void SpotLightEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	SetPosition(wallPosition);
}

void SpotLightEntity::Save(YAML::Emitter& emitter) const
{
	LightSourceEntity::Save(emitter);
	
	glm::vec3 dir = m_rotationMatrix[1];
	
	emitter << YAML::Key << "cutoff" << YAML::Value << m_cutoffAngle
	        << YAML::Key << "penumbra" << YAML::Value << m_penumbraAngle
	        << YAML::Key << "dir" << YAML::Value << YAML::BeginSeq << dir.x << dir.y << dir.z << YAML::EndSeq;
}

void SpotLightEntity::Load(const YAML::Node& node)
{
	LightSourceEntity::Load(node);
	
	SetCutoff(node["cutoff"].as<float>(eg::PI / 4), node["penumbra"].as<float>(eg::PI / 16));
	
	if (const YAML::Node& dirNode = node["dir"])
	{
		SetDirection({ dirNode[0].as<float>(0), dirNode[1].as<float>(0), dirNode[2].as<float>(0) });
	}
}

void SpotLightEntity::EditorRenderSettings()
{
	LightSourceEntity::EditorRenderSettings();
	
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
		drawArgs.primitiveRenderer->AddQuad(positions, GetColor());
	}
}

void SpotLightEntity::EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir)
{
	SetPosition(newPosition);
	SetDirection(DirectionVector(wallNormalDir));
}
