#pragma once

struct WaterQueryResults
{
	int numIntersecting = 0;
	glm::vec3 waterVelocity;
	glm::vec3 buoyancy;
};
