#version 450 core

#pragma variants VDefault VNoSubgroups

#ifndef VNoSubgroups
#extension GL_KHR_shader_subgroup_shuffle : enable
#endif

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(constant_id=0) const uint workGroupSize = 128;

#include "SortWithinWorkgroup.glh"

layout(set = 0, binding = 0) restrict buffer SortDataBuffer
{
	uvec2 inputOutput[];
};

layout(push_constant) uniform PC
{
	uint k;
}
pc;

void main()
{
	uvec2 selfValue = inputOutput[globalIndex];
	selfValue = sortForLargeKSmallJ(pc.k, workGroupSize >> 1, selfValue);
	inputOutput[globalIndex] = selfValue;
}
