#version 450 core

#include "WaterCommon.glh"

layout(local_size_x = 1, local_size_y = MAX_CLOSE, local_size_z = 1) in;

layout(binding=0) readonly buffer ParticlePositions
{
	vec3 particlePos[];
};

layout(binding=1, std430) writeonly buffer ParticleDensities
{
	vec2 particleDensity[];
};

layout(binding=2, r16ui) uniform uimage2D closeParticles;

layout(push_constant) uniform PC
{
	int numParticles;
};

const float MAX_DENSITY = 16384;
const float DENSITY_SCALE = 4294967296.0 / MAX_DENSITY;

shared uint numDensity;
shared uint nearNumDensity;

void main()
{
	uint pIdx = gl_GlobalInvocationID.x;
	if (pIdx >= numParticles)
		return;
	
	if (gl_GlobalInvocationID.y == 0)
	{
		numDensity = 0;
		nearNumDensity = 0;
	}
	
	memoryBarrierShared();
	
	int bIdx = int(imageLoad(closeParticles, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y)).r) - 1;
	if (bIdx != -1)
	{
		float dist = distance(particlePos[pIdx], particlePos[bIdx]);
		float q = 1.0 - min(dist / INFLUENCE_RADIUS, 1.0);
		float q2 = q * q;
		float q3 = q2 * q;
		float q4 = q2 * q2;
		atomicAdd(numDensity, uint(round(q3 * DENSITY_SCALE)));
		atomicAdd(nearNumDensity, uint(round(q4 * DENSITY_SCALE)));
	}
	
	memoryBarrierShared();
	
	if (gl_LocalInvocationID.y == 0)
	{
		particleDensity[pIdx] = vec2(numDensity, nearNumDensity) / DENSITY_SCALE + vec2(1.0);
	}
}
