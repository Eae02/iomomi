#pragma once

#pragma pack(push, 1)
struct SpotLightDrawData
{
	glm::vec3 position;
	float range;
	glm::vec3 direction;
	float penumbraBias;
	glm::vec3 directionL;
	float penumbraScale;
	glm::vec3 radiance;
	float width;
};
#pragma pack(pop)
