#version 450 core

#include "../../WaterConstants.h"

layout(local_size_x = W_COMMON_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict readonly buffer NumOctGroupsPrefixSums
{
	uint numOctGroupsPrefixSumSections[256];
	uint numOctGroupsPrefixSumWithinSection[];
};

layout(set = 0, binding = 1) restrict writeonly buffer OctGroupsRangesBuffer
{
	uint octGroupRanges_out[];
};

uint readPrefixSum(uint index)
{
	return numOctGroupsPrefixSumWithinSection[index] + numOctGroupsPrefixSumSections[index / 256];
}

void main()
{
	uint invoIndex = gl_GlobalInvocationID.x;

	uint inclusivePrefixSum = readPrefixSum(invoIndex);
	uint exclusivePrefixSum = invoIndex == 0 ? 0 : readPrefixSum(invoIndex - 1);

	uint numOctGroups = inclusivePrefixSum - exclusivePrefixSum;

	for (uint i = 0; i < W_MAX_OCT_GROUPS_PER_CELL; i++)
	{
		if (i < numOctGroups)
		{
			octGroupRanges_out[exclusivePrefixSum + i] = invoIndex + i * 8;
		}
	}
}
