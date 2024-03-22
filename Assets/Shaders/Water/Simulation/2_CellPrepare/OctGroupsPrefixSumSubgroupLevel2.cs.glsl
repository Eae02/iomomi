#version 450 core

#extension GL_KHR_shader_subgroup_arithmetic : enable

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

layout(set = 1, binding = 0) uniform Params_UseDynamicOffset
{
	uint numSectionSizesWritten;
};

shared uint subgroupSums[16];

void main()
{
	uint localIndex = gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize;
	
	if (localIndex < subgroupSums.length())
		subgroupSums[localIndex] = 0;
	
	uint value = 0;
	if (localIndex < numSectionSizesWritten)
		value = numOctGroupsSectionSizes[localIndex];
	
	uint exclusiveSumInSubgroup = subgroupExclusiveAdd(value);
	
	// memoryBarrierShared();
	barrier();
	
	if (gl_SubgroupInvocationID == gl_SubgroupSize - 1)
	{
		subgroupSums[gl_SubgroupID] = exclusiveSumInSubgroup + value;
	}
	
	// memoryBarrierShared();
	barrier();
	
	if (gl_SubgroupID == 0)
	{
		uint sumHere = gl_SubgroupInvocationID < subgroupSums.length() ? subgroupSums[gl_SubgroupInvocationID] : 0;
		uint sumBefore = subgroupExclusiveAdd(sumHere);
		if (gl_SubgroupInvocationID < subgroupSums.length())
			subgroupSums[gl_SubgroupInvocationID] = sumBefore;
		if (gl_SubgroupInvocationID == gl_SubgroupSize - 1)
			totalNumOctGroups = sumHere + sumBefore;
	}
	
	// memoryBarrierShared();
	barrier();
	
	numOctGroupsSectionSizes[localIndex] = subgroupSums[gl_SubgroupID] + exclusiveSumInSubgroup;
}
