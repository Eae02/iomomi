#version 450 core

#include "WaterCommon.glh"

layout(local_size_x = 1, local_size_y = MAX_CLOSE, local_size_z = 1) in;

layout(binding=0) readonly buffer ParticlePositions
{
	vec3 particlePos[];
};

layout(binding=1) buffer ParticleVelocities
{
	vec3 particleVel[];
};

layout(binding=2, std430) readonly buffer ParticleDensities
{
	vec2 particleDensity[];
};

layout(binding=3) uniform NoiseBuffer
{
	vec3 noise[128];
};

layout(binding=4, r16ui) uniform uimage2D closeParticles;

layout(push_constant) uniform PC
{
	int numParticles;
	float dt;
};

const float MAX_ACCEL = 32768;
const float ACCEL_SCALE = 2147483648.0 / MAX_ACCEL;

shared int accelX;
shared int accelY;
shared int accelZ;

void main()
{
	uint pIdx = gl_GlobalInvocationID.x;
	if (pIdx >= numParticles)
		return;
	
	if (gl_GlobalInvocationID.y == 0)
	{
		float downForce = -10 * (particleDensity[pIdx].x - AMBIENT_DENSITY) / particleDensity[pIdx].x;
		
		accelX = 0;
		accelY = int(downForce * ACCEL_SCALE);
		accelZ = 0;
	}
	
	memoryBarrierShared();
	
	uint bIdx = int(imageLoad(closeParticles, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y)).r) - 1;
	if (bIdx != -1)
	{
		vec3 sep = particlePos[pIdx] - particlePos[bIdx];
		float dist = length(sep);
		
		if (dist < CORE_RADIUS || abs(sep.x) < CORE_RADIUS || abs(sep.y) < CORE_RADIUS || abs(sep.z) < CORE_RADIUS)
		{
			sep += noise[bIdx % noise.length()];
			dist = length(sep);
		}
		
		float q = max(1.0 - dist / INFLUENCE_RADIUS, 0.0);
		float q2 = q * q;
		float q3 = q2 * q;
		
		float densA     = particleDensity[pIdx].x;
		float densB     = particleDensity[bIdx].x;
		float nearDensA = particleDensity[pIdx].y;
		float nearDensB = particleDensity[bIdx].y;
		float pressure  = STIFFNESS * (densA + densB - 2 * TARGET_NUMBER_DENSITY);
		float pressNear = STIFFNESS * NEAR_TO_FAR * (nearDensA + nearDensB);
		float dReg      = pressure  * q2;
		float dNear     = pressNear * q3;
		vec3 dir        = sep / (dist + 0.00001);
		vec3 accel      = (dReg + dNear) * dir;
		
		ivec3 accelI = ivec3(round(accel * ACCEL_SCALE));
		atomicAdd(accelX, accelI.x);
		atomicAdd(accelY, accelI.y);
		atomicAdd(accelZ, accelI.z);
	}
	
	memoryBarrierShared();
	
	if (gl_GlobalInvocationID.y == 0)
	{
		vec3 accel = vec3(accelX, accelY, accelZ) / ACCEL_SCALE;
		particleVel[pIdx] += accel * dt;
	}
}
