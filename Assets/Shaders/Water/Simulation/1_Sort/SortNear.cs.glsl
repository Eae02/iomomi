#version 450 core

#extension GL_KHR_shader_subgroup_shuffle : enable

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

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
	selfValue = sortForLargeKSmallJ(pc.k, gl_WorkGroupSize.x >> 1, selfValue);
	inputOutput[globalIndex] = selfValue;
}
