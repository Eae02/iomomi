#version 450 core

#include "WaterCommon.glh"

layout(binding = 0, std430) readonly buffer ParticlesBuffer
{
	uvec4 particleData[];
};

layout(push_constant) uniform PC
{
	mat4 transform;
	uint barrierBlockedAxis;
};

void main()
{
	gl_PointSize = 1;
	uvec4 data = particleData[gl_VertexIndex];
	if ((dataBitsGetGravity(data.w) / 2) == barrierBlockedAxis)
	{
		vec3 position = uintBitsToFloat(data.xyz);
		gl_Position = transform * vec4(position, 1.0);
		gl_Position.z = abs(gl_Position.z);
	}
	else
	{
		gl_Position = vec4(100, 100, 100, 1);
	}
}
