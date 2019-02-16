#include "SpotLightEntity.hpp"

static std::atomic<uint64_t> nextInstanceID(0);

SpotLightEntity::SpotLightEntity(const glm::vec3& position, const eg::ColorLin& radiance,
                                 const glm::vec3& direction, float cutoffAngle, float penumbraAngle)
	: m_instanceID(nextInstanceID.fetch_add(1))
{
	SetPosition(position);
	SetDirection(direction);
	SetCutoff(cutoffAngle, penumbraAngle);
	SetRadiance(radiance);
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
	
	float cosMinPAngle = std::cos(cutoffAngle - penumbraAngle);
	float cosMaxPAngle = std::cos(cutoffAngle);
	
	m_penumbraBias = -cosMaxPAngle;
	m_penumbraScale = 1.0f / (cosMinPAngle - cosMaxPAngle);
	
	m_width = std::tan(cutoffAngle);
}

void SpotLightEntity::SetRadiance(const eg::ColorLin& radiance)
{
	m_radiance.r = radiance.r;
	m_radiance.g = radiance.g;
	m_radiance.b = radiance.b;
	
	const float maxChannel = std::max(m_radiance.r, std::max(m_radiance.g, m_radiance.b));
	m_range = std::sqrt(256 * maxChannel + 1.0f);
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
