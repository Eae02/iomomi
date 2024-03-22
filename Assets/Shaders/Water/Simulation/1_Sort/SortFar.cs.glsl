#version 450 core

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict buffer SortDataBuffer
{
	uvec2 inputOutput[];
};

layout(set = 0, binding = 1) restrict readonly buffer IndexBuffer
{
	uint indexBuffer[];
};

layout(set = 1, binding = 0) uniform Params_UseDynamicOffset
{
	uint k;
	uint j;
	uint indexBufferOffset;
}
pc;

void main()
{
	uint selfIndex = indexBuffer[gl_WorkGroupID.x + pc.indexBufferOffset] | gl_LocalInvocationID.x;
	uint otherIndex = selfIndex | pc.j; // selfIndex will have a zero in the bit set in j

	uvec2 selfValue = inputOutput[selfIndex];
	uvec2 otherValue = inputOutput[otherIndex];

	bool kBitSet = (pc.k & selfIndex) != 0;
	bool swap = kBitSet != (selfValue.x > otherValue.x);

	if (swap)
	{
		inputOutput[selfIndex] = otherValue;
		inputOutput[otherIndex] = selfValue;
	}
}
