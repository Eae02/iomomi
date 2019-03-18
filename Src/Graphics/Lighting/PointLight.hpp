#pragma once


struct PointLightDrawData
{
#pragma pack(push, 1)
	struct
	{
		glm::vec3 position;
		float range;
		glm::vec3 radiance;
	} pc;
#pragma pack(pop)
	eg::TextureRef shadowMap;
	uint64_t instanceID;
};
