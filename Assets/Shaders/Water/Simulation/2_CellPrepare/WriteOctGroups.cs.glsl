#version 450 core

#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "../../WaterConstants.h"

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict readonly buffer SortDataBuffer
{
	uint numOctGroupsPrefixSumBuffer[];
};

layout(set = 0, binding = 1) restrict readonly buffer NumOctGroupsBuffer
{
	uint numOctGroupsBuffer[];
};

layout(set = 0, binding = 2) restrict writeonly buffer OctGroupsRangesBuffer
{
	uint octGroupRanges_out[];
};

layout(set = 0, binding = 3) restrict writeonly buffer TotalNumOctGroupsBuffer
{
	uint totalNumOctGroups;
};

layout(push_constant) uniform PC
{
	uvec4 prefixSumLayerOffsets;
	uint lastInvocationIndex;
};

const uint UINT_MAX = 0xFFFFFFFFu;

void main()
{
	uint localIndex = gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize;
	uint globalIndex = gl_WorkGroupID.x * gl_WorkGroupSize.x + localIndex;

	uint numOctGroups = numOctGroupsBuffer[globalIndex];
	uint writeOffset = subgroupExclusiveAdd(numOctGroups);

	for (uint l = 0; l < 4; l++)
	{
		uint psOffset = globalIndex >> ((l + 1) * 5);

		if ((psOffset % 32) != 0 && prefixSumLayerOffsets[l] != UINT_MAX)
		{
			writeOffset += numOctGroupsPrefixSumBuffer[prefixSumLayerOffsets[l] + psOffset - 1];
		}
	}

	if (globalIndex == lastInvocationIndex)
		totalNumOctGroups = writeOffset + numOctGroups;

	for (uint i = 0; i < W_MAX_OCT_GROUPS_PER_CELL; i++)
	{
		if (i < numOctGroups)
		{
			octGroupRanges_out[writeOffset + i] = globalIndex + i * 8;
		}
	}
}
