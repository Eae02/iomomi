#version 450 core

#extension GL_KHR_shader_subgroup_arithmetic : enable

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict buffer SortDataBuffer
{
	uint numOctGroupsBuffer[];
};

layout(push_constant) uniform PC
{
	uint inputOffset;
	uint inputCount;
	uint outputOffset;
}
pc;

const uint UINT_MAX = 0xFFFFFFFFu;

void main()
{
	uint localIndex = gl_SubgroupInvocationID + gl_SubgroupID * gl_SubgroupSize;
	uint globalIndex = gl_WorkGroupID.x * gl_WorkGroupSize.x + localIndex;
	
	bool inRange = globalIndex < pc.inputCount;
	uint value = inRange ? numOctGroupsBuffer[pc.inputOffset + globalIndex] : 0;
	
	uint valuePrefixSum = subgroupInclusiveAdd(value);
	
	if (inRange)
	{
		numOctGroupsBuffer[pc.inputOffset + globalIndex] = valuePrefixSum;
	}
	
	if (gl_SubgroupInvocationID == gl_SubgroupSize - 1 && pc.outputOffset != UINT_MAX)
	{
		numOctGroupsBuffer[pc.outputOffset + gl_WorkGroupID.x] = valuePrefixSum;
	}
}
