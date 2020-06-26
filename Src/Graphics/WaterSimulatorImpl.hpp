#pragma once

//No STL classes can be passed to the simulator implementation!

#include "../World/Dir.hpp"

struct WSINewArgs
{
	glm::ivec3 minBounds;
	glm::ivec3 maxBounds;
	uint8_t* isAirBuffer;
	uint32_t numParticles;
	uint32_t extraParticles;
	const float* particlePositions;
};

struct WaterBlocker
{
	__m128 center;
	__m128 normal;
	__m128 tangent;
	__m128 biTangent;
	float tangentLen;
	float biTangentLen;
	uint8_t blockedGravities;
};

struct WSISimulateArgs
{
	float dt;
	int changeGravityParticle;
	Dir newGravity;
	size_t numWaterBlockers;
	const WaterBlocker* waterBlockers;
};

struct WaterSimulatorImpl;

WaterSimulatorImpl* WSI_New(const WSINewArgs& args);

void WSI_Delete(WaterSimulatorImpl* impl);

uint32_t WSI_GetPositions(WaterSimulatorImpl* impl, void* destination);

int WSI_Query(WaterSimulatorImpl* impl, const eg::AABB& aabb, glm::vec3& waterVelocity, glm::vec3& buoyancy);

void WSI_SwapBuffers(WaterSimulatorImpl* impl);

void WSI_Simulate(WaterSimulatorImpl* impl, const WSISimulateArgs& args);
