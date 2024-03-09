#version 450 core

#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "../../WaterCommon.glh"

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict readonly buffer SortDataBuffer
{
	uvec2 sortedCellIndices[];
};

layout(set = 0, binding = 1) restrict buffer ParticleDataBuffer
{
	uvec4 particleData[];
};

layout(set = 0, binding = 2) restrict writeonly buffer NumOctGroupsBuffer
{
	uint numOctGroups_out[];
};

layout(set = 0, binding = 3) restrict writeonly buffer NumOctGroupsSumBuffer
{
	uint numOctGroupsSum_out[];
};

layout(set = 0, binding = 4, r16ui) restrict writeonly uniform uimage3D cellOffsetsImage;

layout(set = 0, binding = 5) restrict writeonly buffer ParticleDataForCPU_UseDynamicOffset
{
	uvec2 particleDataForCPU[];
};

layout(push_constant) uniform PC
{
	vec3 gridOrigin;
	uint numParticles;
	uint posInOffset;
	uint posOutOffset;
	uint velInOffset;
	uint velOutOffset;
}
pc;

const uint UINT_MAX = 0xFFFFFFFFu;

uvec2 tryReadCell(uint i)
{
	return (i < pc.numParticles) ? sortedCellIndices[i] : uvec2(UINT_MAX);
}

void main()
{
	uint localIndex = gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize;
	uint globalIndex = gl_WorkGroupID.x * gl_WorkGroupSize.x + localIndex;

	uvec2 selfCell = sortedCellIndices[globalIndex];
	bool isEdge = (globalIndex == 0) || sortedCellIndices[globalIndex - 1].x != selfCell.x;

	uvec4 positionData = particleData[pc.posInOffset + selfCell.y];
	vec3 position = uintBitsToFloat(positionData.xyz);
	particleData[pc.posOutOffset + globalIndex] = uvec4(positionData.xyz, selfCell.x);

	uvec4 velData = particleData[pc.velInOffset + selfCell.y];
	particleData[pc.velOutOffset + globalIndex] = velData;

	uvec3 gridCell = getGridCell(position, pc.gridOrigin);

	particleDataForCPU[globalIndex] = uvec2(gridCell.x | (gridCell.y << 10) | (gridCell.z << 20), velData.w);

	uint numOctGroups = 0;
	if (isEdge)
	{
		imageStore(cellOffsetsImage, ivec3(gridCell), uvec4(globalIndex));

		if (tryReadCell(globalIndex + 16).x == selfCell.x)
			numOctGroups = 3;
		else
			numOctGroups = 1;
		if (tryReadCell(globalIndex + numOctGroups * 8).x == selfCell.x)
			numOctGroups += 1;
	}

	numOctGroups_out[globalIndex] = numOctGroups;

	uint numOctGroupsTot = subgroupAdd(numOctGroups);
	if (gl_SubgroupInvocationID == 0)
	{
		numOctGroupsSum_out[globalIndex / gl_SubgroupSize] = numOctGroupsTot;
	}
}
