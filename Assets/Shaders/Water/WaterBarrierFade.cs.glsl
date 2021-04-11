layout(local_size_x=8, local_size_y=8, local_size_z=1) in;

layout(binding=0, r32ui) uniform readonly uimage2D targetDistancesImage;
layout(binding=1, r32f) uniform image2D curDistancesImage;

layout(push_constant) uniform PC
{
	float maxStep;
};

const float UINT_MAX = 4294967296.0;

void main()
{
	float targetDist = float(imageLoad(targetDistancesImage, ivec2(gl_GlobalInvocationID.xy)).r) / (UINT_MAX - 1);
	float curDist = imageLoad(curDistancesImage, ivec2(gl_GlobalInvocationID.xy)).r;
	float newDist = curDist + clamp(targetDist - curDist, -maxStep, maxStep);
	imageStore(curDistancesImage, ivec2(gl_GlobalInvocationID.xy), vec4(newDist));
}
