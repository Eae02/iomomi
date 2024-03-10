#version 450 core

#include "../../WaterConstants.h"

layout(local_size_x = W_OCT_GROUPS_PREFIX_SUM_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict buffer NumOctGroupsPrefixSums
{
	uint numOctGroupsSectionSizes[256];
};

layout(set = 0, binding = 1) restrict writeonly buffer TotalNumOctGroupsBuffer
{
	uint totalNumOctGroups;
};

layout(push_constant) uniform PC
{
	uint numSectionSizesWritten;
};

shared uint prefixSums[256];

void main()
{
	uint invoIndex = gl_LocalInvocationID.x;
	
	uint value = 0;
	if (invoIndex < numSectionSizesWritten)
		value = numOctGroupsSectionSizes[invoIndex];
	
	prefixSums[invoIndex] = value;
	
	// Parallel prefix sum by Hillis & Steele
	for (uint i = 1; i < 256; i <<= 1)
	{
		memoryBarrierShared();
		barrier();
		if (invoIndex >= i)
			prefixSums[invoIndex] += prefixSums[invoIndex - i];
	}
	
	uint inclusiveSum = prefixSums[invoIndex];
	
	if (invoIndex == W_OCT_GROUPS_PREFIX_SUM_WG_SIZE - 1)
		totalNumOctGroups = inclusiveSum;
	
	numOctGroupsSectionSizes[invoIndex] = inclusiveSum - value;
}
