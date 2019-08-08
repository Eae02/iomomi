#version 450 core

#include "WaterCommon.glh"

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(binding=0) buffer ParticlePositions
{
	vec3 particlePos[];
};

layout(binding=1) buffer ParticleVelocities
{
	vec3 particleVel[];
};

const int MAX_PLANES = 8;

layout(binding=2, r8ui) uniform readonly uimage3D voxelSoliditySampler;

layout(push_constant) uniform PC
{
	int numParticles;
	float dt;
	ivec3 voxelGridMin;
};

const ivec3 voxelNormals[] = ivec3[] (
	ivec3(-1, 0, 0), ivec3(1, 0, 0),
	ivec3(0, -1, 0), ivec3(0, 1, 0),
	ivec3(0, 0, -1), ivec3(0, 0, 1)
);

const ivec3 voxelTangents[] = ivec3[] (
	ivec3(0, 0, 1), ivec3(0, 0, 1),
	ivec3(1, 0, 0), ivec3(1, 0, 0),
	ivec3(0, 1, 0), ivec3(0, 1, 0)
);

void main()
{
	uint pIdx = gl_GlobalInvocationID.x;
	if (pIdx >= numParticles)
		return;
	
	float speed = length(particleVel[pIdx]);
	if (speed > SPEED_LIMIT)
	{
		particleVel[pIdx] *= SPEED_LIMIT / speed;
	}
	
	particlePos[pIdx] += particleVel[pIdx] * dt;
	
	ivec3 centerVx = ivec3(floor(particlePos[pIdx])) - voxelGridMin;
	uint centerSolid = imageLoad(voxelSoliditySampler, centerVx).r;
	
	for (int t = 0; t < 3; t++)
	{
		float minDist = 0;
		vec3 displaceNormal;
		
		const int CHECK_RAD = 2;
		for (int x = -CHECK_RAD; x <= CHECK_RAD; x++)
		for (int y = -CHECK_RAD; y <= CHECK_RAD; y++)
		for (int z = -CHECK_RAD; z <= CHECK_RAD; z++)
		{
			ivec3 voxelCoord = centerVx + ivec3(x, y, z);
			uint neighIsAir = imageLoad(voxelSoliditySampler, voxelCoord).r;
			if (neighIsAir == 1)
				continue;
			
			vec3 voxelCoordWS = vec3(voxelCoord + voxelGridMin);
			
			for (int n = 0; n < voxelNormals.length(); n++)
			{
				if (imageLoad(voxelSoliditySampler, voxelCoord + voxelNormals[n]).r == 0)
					continue;
				
				vec3 planePoint = (voxelCoordWS + 0.5 + vec3(voxelNormals[n]) * 0.5);
				float distC = dot(particlePos[pIdx] - planePoint, vec3(voxelNormals[n]));
				float distE = distC - PARTICLE_RADIUS;
				
				if (distE < minDist)
				{
					vec3 t = voxelTangents[n];
					vec3 bt = cross(t, vec3(voxelNormals[n]));
					
					vec3 iPos = particlePos[pIdx] + distC * vec3(voxelNormals[n]);
					float iDotT = dot(iPos - planePoint, t);
					float iDotBT = dot(iPos - planePoint, bt);
					float iDotN = dot(iPos - planePoint, voxelNormals[n]);
					
					if (abs(iDotT) <= 0.5 && abs(iDotBT) <= 0.5 && abs(iDotN) < 0.8)
					{
						minDist = distE;
						displaceNormal = voxelNormals[n];
					}
				}
			}
		}
		
		if (minDist < 0.0f)
		{
			float speedNormal = dot(particleVel[pIdx], displaceNormal);
			vec3 impulse = speedNormal * displaceNormal * minDist;
			particleVel[pIdx] += impulse * IMPACT_COEFFICIENT;
		}
		particlePos[pIdx] -= displaceNormal * minDist;
	}
}
