#include "SpotLight.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"

SpotLight::SpotLight(const eg::ColorSRGB& color, float intensity, float cutoffAngle, float penumbraAngle)
	: LightSource(color, intensity)
{
	SetDirection(glm::vec3(0, 1, 0));
	SetCutoff(cutoffAngle, penumbraAngle);
}

void SpotLight::SetDirection(const glm::vec3& direction)
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

void SpotLight::SetCutoff(float cutoffAngle, float penumbraAngle)
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

SpotLightDrawData SpotLight::GetDrawData(const glm::vec3& position) const
{
	SpotLightDrawData data;
	data.position = position;
	data.range = Range();
	data.width = m_width;
	data.direction = Direction();
	data.directionL = DirectionL();
	data.penumbraBias = m_penumbraBias;
	data.penumbraScale = m_penumbraScale;
	data.radiance = Radiance();
	return data;
}

const glm::vec3 coneVertices[] = 
{
	glm::vec3(1, 1, 0),
	glm::vec3(-1, 1, 0),
	glm::vec3(0, 1, 1),
	glm::vec3(0, 1, -1)
};

void SpotLight::DrawCone(PrimitiveRenderer& renderer, const glm::vec3& position) const
{
	glm::vec3 forward = Direction();
	for (const glm::vec3& v : coneVertices)
	{
		float LINE_WIDTH = 0.01f;
		glm::vec3 dl = glm::cross(v, forward) * LINE_WIDTH;
		glm::vec3 positions[] = { dl, -dl, v + dl, v - dl };
		for (glm::vec3& pos : positions)
		{
			pos = position + m_rotationMatrix * (glm::vec3(m_width, 1.0f, m_width) * pos);
		}
		renderer.AddQuad(positions, GetColor());
	}
}
