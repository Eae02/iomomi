#pragma once

#include "LightSource.hpp"

struct PointLightDrawData
{
#pragma pack(push, 1)
	struct
	{
		glm::vec3 position;
		float range;
		glm::vec3 radiance;
		float invRange;
	} pc;
#pragma pack(pop)
	eg::TextureRef shadowMap;
	uint64_t instanceID;
	bool castsShadows;
};

class PointLight : public LightSource
{
public:
	PointLight()
		: LightSource(eg::ColorSRGB(1.0f, 1.0f, 1.0f), 20.0f) { }
	
	PointLight(const eg::ColorSRGB& color, float intensity)
		: LightSource(color, intensity) { }
	
	PointLightDrawData GetDrawData(const glm::vec3& position) const;
	
	bool castsShadows = true;
};
