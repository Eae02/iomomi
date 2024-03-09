#version 450 core

#extension GL_KHR_shader_subgroup_vote : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_clustered : enable

layout(push_constant) uniform PC
{
	vec3 gridOrigin;
	uint positionsBufferOffset;
	uint velocityBufferOffset;
	uint numParticles;
	float dt;
	uint randomOffsetsBase;
};

#define NEAR_PAIR_RES_T vec3
#include "ForEachNear.cs.glh"

layout(set = 1, binding = 0) restrict readonly buffer DensityBuffer
{
	vec2 densityBuffer[];
};

layout(set = 1, binding = 1) uniform RandomOffsetsBuffer
{
	uint randomOffsetsBuffer[256];
};

vec3 sampleRandomOffset(uint i1, uint i2)
{
	uint hash = i1 + (i2 * 17);
	uint index = hash ^ (hash >> 8) ^ (hash >> 16);
	uint offsetU32 = randomOffsetsBuffer[(index + randomOffsetsBase) & 0xFF];
	return unpackSnorm4x8(offsetU32).xyz;
}

vec3 processNearPair(uint i1, uint i2, vec3 pos1, vec3 pos2)
{
	vec3 sep = pos1 - pos2;
	float dist = length(sep);

	const float MIN_DIST_EPSILON = 0.005;
	if (dist < MIN_DIST_EPSILON)
	{
		sep = sampleRandomOffset(i1, i2) * MIN_DIST_EPSILON;
		dist = length(sep);
	}

	float q = max(1.0 - dist / W_INFLUENCE_RADIUS, 0.0);
	float q2 = q * q;
	float q3 = q * q2;

	vec2 density1 = densityBuffer[i1];
	vec2 density2 = densityBuffer[i2];

	const float DIST_EPSILON = 1E-6;

	float pressure = W_STIFFNESS * (density1.x + density2.x - 2.0 * W_TARGET_DENSITY);
	float pressureNear = W_STIFFNESS * W_NEAR_TO_FAR * (density1.y + density2.y);
	float accelScale = (pressure * q2 + pressureNear * q3) / (dist + DIST_EPSILON);
	return sep * accelScale;
}

void initialize(uint particleIndex, vec3 pos) {}

void processResult(vec3 accel, uint particleIndex)
{
	float density = densityBuffer[particleIndex].x;
	float relativeDensity = (density - W_AMBIENT_DENSITY) / density;

	uvec4 velAndGravity = particleData[velocityBufferOffset + particleIndex];
	uint gravityIdx = dataBitsGetGravity(velAndGravity.w);

	vec3 gravity = vec3(0.0);
	gravity[gravityIdx / 2] = (gravityIdx % 2 != 0) ? -W_GRAVITY : W_GRAVITY;

	accel += gravity * relativeDensity;

	vec3 newVelocity = uintBitsToFloat(velAndGravity.xyz) + accel * dt;

	particleData[velocityBufferOffset + particleIndex] = uvec4(floatBitsToUint(newVelocity), velAndGravity.w);
}
