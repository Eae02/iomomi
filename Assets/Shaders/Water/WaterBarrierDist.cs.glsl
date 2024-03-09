layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

layout(binding=0, std430) readonly buffer ParticlePosBuf
{
	uvec4 particleData[];
};

layout(binding=1, r32ui) uniform uimage2D distancesImage;

layout(push_constant) uniform PC
{
	mat4 barrierTransform;
	uint barrierBlockedGravity;
};

const float UINT_MAX = 4294967296.0;

#include "WaterCommon.glh"

void main()
{
	uvec4 data = particleData[gl_GlobalInvocationID.x];
	if ((dataBitsGetGravity(data.w) / 2) == barrierBlockedGravity)
	{
		vec3 position = uintBitsToFloat(data.xyz);
		vec3 barrierWP = (barrierTransform * vec4(gl_GlobalInvocationID.yz, 0, 1)).xyz;
		float dst = distance(barrierWP, position);
		uint dstI = uint(min(dst, 1) * (UINT_MAX - 1));
		imageAtomicMin(distancesImage, ivec2(gl_GlobalInvocationID.yz), dstI);
	}
}
