#version 450 core

#include "WaterCommon.glh"

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(binding=0) readonly buffer ParticlePositions
{
	vec3 particlePos[];
};

layout(binding=1, std430) buffer NumCloseBuffer
{
	int numClose[];
};

layout(binding=2, r16ui) uniform uimage2D closeParticles;

layout(push_constant) uniform PC
{
	int numParticles;
};

void main()
{
	uint a = gl_GlobalInvocationID.x;
	uint b = gl_GlobalInvocationID.y;
	
	if (a >= numParticles || a == b)
		return;
	
	if (b < a)
	{
		if (b == numParticles / 2)
			return;
		a = numParticles - a - 1;
		b = numParticles - b - 1;
	}
	
	if (distance(particlePos[a], particlePos[b]) < INFLUENCE_RADIUS)
	{
		uint idxA = atomicAdd(numClose[a], 1);
		uint idxB = atomicAdd(numClose[b], 1);
		imageStore(closeParticles, ivec2(a, idxA), uvec4(b + 1));
		imageStore(closeParticles, ivec2(b, idxB), uvec4(a + 1));
	}
}
