#version 450 core

#include "../WaterCommon.glh"

layout(local_size_x = W_COMMON_WG_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) restrict buffer ParticleDataBuffer
{
	uvec4 velParticleData[];
};

layout(set = 0, binding = 1) restrict readonly buffer ChangeGravityBitsBuffer
{
	uint changeGravityBits[];
};

layout(set = 1, binding = 0) uniform Params_UseDynamicOffset
{
	uint newGravity;
};

void main()
{
	uint particleDataBits = velParticleData[gl_GlobalInvocationID.x].w;
	uint id = dataBitsGetPersistentID(particleDataBits);
	bool change = (changeGravityBits[id / 32] & (1u << (id % 32))) != 0;

	if (change)
	{
		velParticleData[gl_GlobalInvocationID.x].w = encodeDataBits(newGravity, W_MAX_GLOW_TIME, id);
	}
}
