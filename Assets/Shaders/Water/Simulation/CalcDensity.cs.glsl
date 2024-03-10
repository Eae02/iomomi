#version 450 core

#pragma variants VSubgroups VSubgroupsNoClustered VNoSubgroups

layout(push_constant) uniform PC
{
	vec3 gridOrigin;
	uint positionsBufferOffset;
	uint numParticles;
};

#define NEAR_PAIR_RES_T vec2
#include "ForEachNear.cs.glh"

vec2 processNearPair(uint i1, uint i2, vec3 pos1, vec3 pos2)
{
	float q = max(1.0 - distance(pos1, pos2) / W_INFLUENCE_RADIUS, 0.0);
	float q2 = q * q;
	float q3 = q * q2;
	float q4 = q2 * q2;
	return vec2(q3, q4);
}

layout(set = 1, binding = 0) restrict writeonly buffer DensityOutBuffer
{
	vec2 densityOut[];
};

void initialize(uint particleIndex, vec3 pos) {}

void processResult(vec2 result, uint particleIndex)
{
	densityOut[particleIndex] = result + vec2(1.0);
}
