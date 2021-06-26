#pragma once

#include "../../World/Dir.hpp"
#include "WaterPumpDescription.hpp"

#include <span>

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
	float gameTime;
	
	bool shouldChangeParticleGravity;
	bool changeGravityParticleHighlightOnly;
	glm::vec3 changeGravityParticlePos;
	Dir newGravity;
	
	std::span<const WaterBlocker> waterBlockers;
	std::span<const WaterPumpDescription> waterPumps;
};

struct WaterSimulatorImpl;

WaterSimulatorImpl* WSI_New(const WSINewArgs& args);

void WSI_Delete(WaterSimulatorImpl* impl);

uint32_t WSI_CopyToOutputBuffer(WaterSimulatorImpl* impl);
void WSI_SortOutputBuffer(WaterSimulatorImpl* impl, uint32_t numParticles, const float* cameraPos4);
void* WSI_GetOutputBuffer(WaterSimulatorImpl* impl);

void* WSI_GetGravitiesOutputBuffer(WaterSimulatorImpl* impl, uint32_t& versionOut);

int WSI_Query(WaterSimulatorImpl* impl, const eg::AABB& aabb, glm::vec3& waterVelocity, glm::vec3& buoyancy);

void WSI_SwapBuffers(WaterSimulatorImpl* impl);

void WSI_Simulate(WaterSimulatorImpl* impl, const WSISimulateArgs& args);
