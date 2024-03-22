#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

#include "../WaterCommon.glh"

layout(local_size_x = W_COMMON_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict readonly buffer PositionsIn
{
	uvec4 positionsIn[];
};

layout(set = 0, binding = 1) restrict buffer PositionsOut
{
	uvec4 positionsOut[];
};

layout(set = 0, binding = 2) restrict buffer VelocitiesBuffer
{
	uvec4 velocities[];
};

layout(set = 0, binding = 3) uniform utexture3D voxelDataTexture;

struct CollisionQuad
{
	vec3 center;
	uint blockedGravities;
	uint normal;
	uint tangent;
	float tangentLen;
	float bitangentLen;
};

layout(set = 0, binding = 4) restrict readonly buffer CollisionQuadsBuffer
{
	CollisionQuad collisionQuads[];
};

layout(set = 0, binding = 5) uniform ParamsUB
{
	WaterSimParameters params;
};

const float MAX_MOVE_DIST_PER_FRAME = 0.2;
const uint MAX_COLLISION_RESOVE_ITERATIONS = 3;
const uint NUM_MOVE_STEPS = 3;

vec3 capMove(vec3 move)
{
	float moveLen2 = dot(move, move);
	if (moveLen2 > MAX_MOVE_DIST_PER_FRAME * MAX_MOVE_DIST_PER_FRAME)
	{
		move *= MAX_MOVE_DIST_PER_FRAME / sqrt(moveLen2);
	}
	return move;
}

struct VoxelData
{
	uint solidMask;
	ivec3 collisionQuadIndices;
};

VoxelData sampleVoxelData(ivec3 pos)
{
	ivec3 sampleCoord = pos - params.voxelMinBounds;
	uint dataU32;
	if (all(greaterThanEqual(sampleCoord, ivec3(0))) && all(lessThan(sampleCoord, textureSize(voxelDataTexture, 0))))
	{
		dataU32 = texelFetch(voxelDataTexture, sampleCoord, 0).r;
	}
	else
	{
		dataU32 = 0xFF;
	}

	VoxelData data;
	data.solidMask = dataU32 & 0xFF;
	data.collisionQuadIndices = ivec3((dataU32 >> 8) & 0xFF, (dataU32 >> 16) & 0xFF, (dataU32 >> 24) & 0xFF) - 1;
	return data;
}

const uvec3 voxelPlanePairs[][2] = uvec3[][2](
	uvec3[](uvec3(0, 0, 0), uvec3(1, 0, 0)), uvec3[](uvec3(0, 1, 0), uvec3(1, 1, 0)),
	uvec3[](uvec3(0, 0, 0), uvec3(0, 1, 0)), uvec3[](uvec3(1, 0, 0), uvec3(1, 1, 0)),

	uvec3[](uvec3(0, 0, 1), uvec3(1, 0, 1)), uvec3[](uvec3(0, 1, 1), uvec3(1, 1, 1)),
	uvec3[](uvec3(0, 0, 1), uvec3(0, 1, 1)), uvec3[](uvec3(1, 0, 1), uvec3(1, 1, 1)),

	uvec3[](uvec3(0, 0, 0), uvec3(0, 0, 1)), uvec3[](uvec3(1, 0, 0), uvec3(1, 0, 1)),
	uvec3[](uvec3(0, 1, 0), uvec3(0, 1, 1)), uvec3[](uvec3(1, 1, 0), uvec3(1, 1, 1))
);

uint getVoxelMask(uvec3 voxel)
{
	uint bit = voxel.x + voxel.y * 2 + voxel.z * 4;
	return 1 << bit;
}

void quadCheckCollision(
	inout float minDist, inout vec3 displaceNormal, vec3 position, vec3 quadPoint, vec3 quadNormal, vec4 quadTangent,
	vec4 quadBitangent, bool requirePositiveDistance
)
{
	float distC = dot((position - quadPoint), quadNormal);
	if (distC < 0 && requirePositiveDistance)
		return;

	float distE = distC - W_PARTICLE_COLLIDE_RADIUS * 0.5;

	vec3 iPos = position + distC * quadNormal - quadPoint;
	float iDotT = dot(iPos, quadTangent.xyz);
	float iDotBT = dot(iPos, quadBitangent.xyz);
	float iDotN = dot(iPos, quadNormal);

	if (distE < minDist && abs(iDotT) <= quadTangent.w && abs(iDotBT) <= quadBitangent.w && abs(iDotN) < 0.8)
	{
		minDist = distE - 0.001;
		displaceNormal = quadNormal;
	}
}

void main()
{
	uint particleIndex = gl_GlobalInvocationID.x;
	uvec4 velAndGravity = velocities[particleIndex];

	vec3 velocity = uintBitsToFloat(velAndGravity.xyz);
	uint dataBits = velAndGravity.w;
	
	uint gravityIndex = dataBitsGetGravity(dataBits);
	uint gravityMask = 1 << gravityIndex;

	vec3 position = uintBitsToFloat(positionsIn[particleIndex].xyz);

	vec3 move = capMove(velocity * params.dt);

	for (uint s = 0; s < NUM_MOVE_STEPS; s++)
	{
		position += move / float(NUM_MOVE_STEPS);

		for (uint i = 0; i < MAX_COLLISION_RESOVE_ITERATIONS; i++)
		{
			float minDist = 0;
			vec3 displaceNormal = vec3(0);

			ivec3 voxelCoord = ivec3(floor(position - W_PARTICLE_COLLIDE_RADIUS));
			VoxelData voxelData = sampleVoxelData(voxelCoord);

			for (uint p = 0; p < 3; p++)
			{
				if (voxelData.collisionQuadIndices[p] == -1)
					continue;

				CollisionQuad quad = collisionQuads[voxelData.collisionQuadIndices[p]];
				if ((quad.blockedGravities & gravityMask) == 0)
					continue;

				vec3 tangent = unpackSnorm4x8(quad.tangent).xyz;
				vec3 normal = unpackSnorm4x8(quad.normal).xyz;
				vec3 bitangent = cross(normal, tangent);

				quadCheckCollision(
					minDist, displaceNormal, position, quad.center + normal * 0.1, normal, vec4(tangent, quad.tangentLen),
					vec4(bitangent, quad.bitangentLen), true
				);

				quadCheckCollision(
					minDist, displaceNormal, position, quad.center - normal * 0.1, -normal, vec4(tangent, quad.tangentLen),
					vec4(bitangent, quad.bitangentLen), true
				);
			}

			for (uint p = 0; p < voxelPlanePairs.length(); p++)
			{
				bool solid0 = (voxelData.solidMask & getVoxelMask(voxelPlanePairs[p][0])) != 0;
				bool solid1 = (voxelData.solidMask & getVoxelMask(voxelPlanePairs[p][1])) != 0;

				if (solid0 == solid1)
					continue;

				ivec3 normal = ivec3(voxelPlanePairs[p][1]) - ivec3(voxelPlanePairs[p][0]);
				if (solid1)
					normal = -normal;

				vec3 tangent = (normal.x == 0) ? vec3(1, 0, 0) : vec3(0, 1, 0);
				vec3 normalF = vec3(normal);
				vec3 bitangent = cross(normalF, tangent);

				vec3 center0 = vec3(voxelCoord + ivec3(voxelPlanePairs[p][0])) + 0.5;
				vec3 center1 = vec3(voxelCoord + ivec3(voxelPlanePairs[p][1])) + 0.5;
				vec3 planePoint = (center0 + center1) / 2.0;

				const float TANGENT_LEN = 0.5 + 0.001;
				quadCheckCollision(
					minDist, displaceNormal, position, planePoint, normalF, vec4(tangent, TANGENT_LEN),
					vec4(bitangent, TANGENT_LEN), false
				);
			}

			float vDotDisplace = dot(velocity, displaceNormal) * (1.0 + W_ELASTICITY);
			velocity -= displaceNormal * vDotDisplace;

			position -= displaceNormal * minDist;
		}
	}

	position = clamp(position, vec3(params.voxelMinBounds), vec3(params.voxelMaxBounds));

	uint newGlowTime = uint(max(int(dataBitsGetGlowTime(dataBits)) - params.glowTimeSubtract, 0));
	dataBitsSetGlowTime(dataBits, newGlowTime);

	velocities[particleIndex] = uvec4(floatBitsToUint(velocity), dataBits);
	positionsOut[particleIndex] = uvec4(floatBitsToUint(position), dataBits);
}
