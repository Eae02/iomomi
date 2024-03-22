#version 450 core

#include "../../WaterCommon.glh"

layout(local_size_x = W_COMMON_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict readonly buffer SortDataBuffer
{
	uvec2 sortedCellIndices[];
};

layout(set = 0, binding = 1, r32ui) restrict writeonly uniform uimage3D cellOffsetsImage;

layout(set = 0, binding = 2) restrict writeonly buffer NumOctGroupsBuffer
{
	uint numOctGroups_out[];
};

layout(set = 0, binding = 3) restrict readonly buffer PositionsIn
{
	uvec4 positionsIn[];
};

layout(set = 0, binding = 4) restrict writeonly buffer PositionsOut
{
	uvec4 positionsOut[];
};

layout(set = 0, binding = 5) restrict readonly buffer VelocitiesIn
{
	uvec4 velocitiesIn[];
};

layout(set = 0, binding = 6) restrict writeonly buffer VelocitiesOut
{
	uvec4 velocitiesOut[];
};

layout(set = 0, binding = 7) uniform ParamsUB
{
	WaterSimParameters params;
};

layout(set = 1, binding = 0) restrict writeonly buffer ParticleDataForCPU
{
	uvec2 particleDataForCPU[];
};

const uint UINT_MAX = 0xFFFFFFFFu;

uvec2 tryReadCell(uint i)
{
	return (i < params.numParticles) ? sortedCellIndices[i] : uvec2(UINT_MAX);
}

void main()
{
	uint invoIndex = gl_GlobalInvocationID.x;

	uvec2 selfCell = sortedCellIndices[invoIndex];
	bool isEdge = (invoIndex == 0) || sortedCellIndices[invoIndex - 1].x != selfCell.x;

	uvec4 positionData = positionsIn[selfCell.y];
	vec3 position = uintBitsToFloat(positionData.xyz);
	positionsOut[invoIndex] = uvec4(positionData.xyz, selfCell.x);

	uvec4 velData = velocitiesIn[selfCell.y];
	velocitiesOut[invoIndex] = velData;

	uvec3 gridCell = getGridCell(position, params.voxelMinBounds);

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
