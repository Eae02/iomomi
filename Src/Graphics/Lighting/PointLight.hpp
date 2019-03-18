#pragma once

#pragma pack(push, 1)
struct PointLightDrawData
{
	glm::vec3 position;
	float range;
	glm::vec3 radiance;
	int32_t shadowMapIndex;
	uint64_t instanceID;
};
#pragma pack(pop)
