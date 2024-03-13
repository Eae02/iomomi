#version 450 core

#pragma variants VDefault VNoSubgroups

#ifndef VNoSubgroups
#extension GL_KHR_shader_subgroup_shuffle : enable
#endif

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

#include "SortWithinWorkgroup.glh"
#include "../../WaterCommon.glh"

layout(set = 0, binding = 0) restrict readonly buffer PositionsBuffer
{
	uvec4 inputPositions[];
};

layout(set = 0, binding = 1) restrict writeonly buffer OutputBuffer
{
	uvec2 outputBuffer[];
};

layout(push_constant) uniform PC
{
	vec3 gridOrigin;
	uint numParticles;
}
pc;

void main()
{
	uvec2 selfValue = uvec2(0xFFFFFFFFu);

	if (globalIndex < pc.numParticles)
	{
		vec3 pos = uintBitsToFloat(inputPositions[globalIndex].xyz);
		uvec3 gridCell = getGridCell(pos, pc.gridOrigin);
		uint gridCellZCurveIndex = zCurveIndex10(gridCell);

		selfValue = uvec2(gridCellZCurveIndex, globalIndex);
	}

	for (uint k = 2; k <= MAX_SUPPORTED_SUBGROUP_SIZE; k <<= 1)
	{
		for (uint jb = k >> 1; jb > 0; jb >>= 1)
		{
			COMPARE_SORT_SUBGROUP(k, jb)
		}
	}

	for (uint k = MAX_SUPPORTED_SUBGROUP_SIZE << 1; k <= gl_WorkGroupSize.x; k <<= 1)
	{
		selfValue = sortForLargeKSmallJ(k, k >> 1, selfValue);
	}

	outputBuffer[globalIndex] = selfValue;
}
