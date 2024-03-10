#version 450 core

#include "../../WaterCommon.glh"

layout(local_size_x = W_COMMON_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

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

layout(set = 0, binding = 3, r16ui) restrict writeonly uniform uimage3D cellOffsetsImage;

layout(set = 0, binding = 4) restrict writeonly buffer ParticleDataForCPU_UseDynamicOffset
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
	uint invoIndex = gl_GlobalInvocationID.x;

	uvec2 selfCell = sortedCellIndices[invoIndex];
	bool isEdge = (invoIndex == 0) || sortedCellIndices[invoIndex - 1].x != selfCell.x;

	uvec4 positionData = particleData[pc.posInOffset + selfCell.y];
	vec3 position = uintBitsToFloat(positionData.xyz);
	particleData[pc.posOutOffset + invoIndex] = uvec4(positionData.xyz, selfCell.x);

	uvec4 velData = particleData[pc.velInOffset + selfCell.y];
	particleData[pc.velOutOffset + invoIndex] = velData;

	uvec3 gridCell = getGridCell(position, pc.gridOrigin);

	particleDataForCPU[invoIndex] = uvec2(gridCell.x | (gridCell.y << 10) | (gridCell.z << 20), velData.w);

	uint numOctGroups = 0;
	if (isEdge)
	{
		imageStore(cellOffsetsImage, ivec3(gridCell), uvec4(invoIndex));

		if (tryReadCell(invoIndex + 16).x == selfCell.x)
			numOctGroups = 3;
		else
			numOctGroups = 1;
		if (tryReadCell(invoIndex + numOctGroups * 8).x == selfCell.x)
			numOctGroups += 1;
	}

	numOctGroups_out[invoIndex] = numOctGroups;
}
