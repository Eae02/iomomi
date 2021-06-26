#pragma once

#include <glm/glm.hpp>

struct WaterPumpDescription
{
	glm::vec3 source;
	glm::vec3 dest;
	float particlesPerSecond;
	float maxInputDistSquared;
	float maxOutputDist;
};
