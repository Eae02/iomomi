#version 450 core

#include "../../WaterConstants.h"

layout(local_size_x = W_OCT_GROUPS_PREFIX_SUM_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict readonly buffer NumOctGroupsIn
{
	uint numOctGroupsBuffer[];
};

layout(set = 0, binding = 1) restrict writeonly buffer NumOctGroupsPrefixSums
{
	uint numOctGroupsSectionSizes[256];
	uint numOctGroupsPrefixSumWithinSection[];
};

shared uint prefixSums[256];

void main()
{
	uint localIndex = gl_LocalInvocationID.x;
	prefixSums[localIndex] = numOctGroupsBuffer[gl_GlobalInvocationID.x];
	
	// Parallel prefix sum by Hillis & Steele
	for (uint i = 1; i < 256; i <<= 1)
	{
		memoryBarrierShared();
		barrier();
		if (localIndex >= i)
			prefixSums[localIndex] += prefixSums[localIndex - i];
	}
	
	uint inclusiveSum = prefixSums[localIndex];
	
	if (localIndex == W_OCT_GROUPS_PREFIX_SUM_WG_SIZE - 1)
		numOctGroupsSectionSizes[gl_WorkGroupID.x] = inclusiveSum;
	
	numOctGroupsPrefixSumWithinSection[gl_GlobalInvocationID.x] = inclusiveSum;
}
