#version 450 core

#pragma variants VSubgroups VSubgroupsNoClustered VNoSubgroups

#define NEAR_PAIR_RES_T vec3
#include "ForEachNear.cs.glh"

layout(set = 1, binding = 0) restrict readonly buffer VelocitiesIn
{
	uvec4 velocitiesIn[];
};

layout(set = 1, binding = 1) restrict writeonly buffer VelocitiesOut
{
	uvec4 velocitiesOut[];
};

vec3 currentVel;
uint currentGravity;

void initialize(uint particleIndex, vec3 pos)
{
	uvec4 vel4 = velocitiesIn[particleIndex];
	currentVel = uintBitsToFloat(vel4.xyz);
	currentGravity = vel4.w;
}

vec3 processNearPair(uint i1, uint i2, vec3 pos1, vec3 pos2)
{
	vec3 vel2 = uintBitsToFloat(velocitiesIn[i2].xyz);
	
	vec3 sep = pos1 - pos2;
	float dist = length(sep);
	vec3 sepDir = sep / dist;
	
	float velSep = dot(currentVel - vel2, sepDir);
	float infl = max(1 - dist / W_INFLUENCE_RADIUS, 0);
	float velSep1 = dot(currentVel, sepDir);
	float velSep2 = dot(vel2, sepDir);
	float velSepTarget = (velSep1 + velSep2) * 0.5;
	float diffSep1 = velSepTarget - velSep1;
	float changeSep1 = W_RADIAL_VISCOSITY_GAIN * diffSep1 * infl;
	return (velSep < 0.0) ? (changeSep1 * sepDir) : vec3(0.0);
}

void processResult(vec3 velDiff, uint particleIndex)
{
	const float VEL_SCALE = 0.998; //TODO: dt?
	vec3 newVel = (currentVel + velDiff) * VEL_SCALE;
	velocitiesOut[particleIndex] = uvec4(floatBitsToUint(newVel), currentGravity);
}
