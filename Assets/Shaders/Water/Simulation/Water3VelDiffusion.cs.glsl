#version 450 core

#include "WaterCommon.glh"

layout(local_size_x = 1, local_size_y = MAX_CLOSE, local_size_z = 1) in;

layout(binding=0) readonly buffer ParticlePositions
{
	vec3 particlePos[];
};

layout(binding=1) readonly buffer ParticleVelocitiesIn
{
	vec3 particleVelIn[];
};

layout(binding=2) writeonly buffer ParticleVelocitiesOut
{
	vec3 particleVelOut[];
};

layout(binding=3, r16ui) uniform uimage2D closeParticles;

layout(push_constant) uniform PC
{
	int numParticles;
};

const float MAX_VEL = 32768;
const float VEL_SCALE = 2147483648.0 / MAX_VEL;

shared int velX;
shared int velY;
shared int velZ;

void main()
{
	uint pIdx = gl_GlobalInvocationID.x;
	if (pIdx >= numParticles)
		return;
	
	if (gl_GlobalInvocationID.y == 0)
	{
		velX = 0;
		velY = 0;
		velZ = 0;
	}
	
	memoryBarrierShared();
	
	vec3 velA = particleVelIn[pIdx];
	vec3 posA = particlePos[pIdx];
	
	uint bIdx = int(imageLoad(closeParticles, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y)).r) - 1;
	if (bIdx != -1)
	{
		float dist = distance(posA, particlePos[bIdx]);
		vec3 velB = particleVelIn[bIdx];
		vec3 sepDir = normalize(posA - particlePos[bIdx]);
		vec3 velDiff = velA - velB;
		float velSep = dot(velDiff, sepDir);
		if (velSep < 0.0)
		{
			// Particles are approaching.
			float infl         = max(1.0 - dist / INFLUENCE_RADIUS, 0.0);
			float velSepA      = dot(velA, sepDir);
			float velSepB      = dot(velB, sepDir);
			float velSepTarget = (velSepA + velSepB) * 0.5;
			float diffSepA     = velSepTarget - velSepA;
			float changeSepA   = RADIAL_VISCOSITY_GAIN * diffSepA * infl;
			vec3 changeA       = changeSepA * sepDir;
			ivec3 changeAI     = ivec3(round(changeA * VEL_SCALE));
			atomicAdd(velX, changeAI.x);
			atomicAdd(velY, changeAI.y);
			atomicAdd(velZ, changeAI.z);
		}
	}
	
	memoryBarrierShared();
	
	if (gl_LocalInvocationID.x == 0)
	{
		particleVelOut[pIdx] = particleVelIn[pIdx] + vec3(velX, velY, velZ) / VEL_SCALE;
	}
}
