layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

layout(binding=0, std430) readonly buffer ParticlePosBuf
{
	vec4 particlePositions[];
};

layout(binding=1, std430) readonly buffer ParticleGravityBuf
{
	uint particleGravities[];
};

layout(binding=2, r32ui) uniform uimage2D distancesImage;

layout(push_constant) uniform PC
{
	mat4 barrierTransform;
	uint barrierBlockedGravity;
};

const float UINT_MAX = 4294967296.0;

void main()
{
	uint bitShift = 1 + (gl_GlobalInvocationID.x % 4) * 8;
	uint particleDown = ((particleGravities[gl_GlobalInvocationID.x / 4]) >> bitShift) & 0xFF;
	if (particleDown == barrierBlockedGravity)
	{
		vec3 barrierWP = (barrierTransform * vec4(gl_GlobalInvocationID.yz, 0, 1)).xyz;
		float dst = distance(barrierWP, particlePositions[gl_GlobalInvocationID.x].xyz);
		uint dstI = uint(min(dst, 1) * (UINT_MAX - 1));
		imageAtomicMin(distancesImage, ivec2(gl_GlobalInvocationID.yz), dstI);
	}
}
