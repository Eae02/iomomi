#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in mat3x4 worldTransform_in;

layout(push_constant) uniform PC
{
	mat4 viewProj;
	uint flags;
};

void main()
{
	mat4 worldTransform = transpose(mat4(worldTransform_in));
	vec3 worldPos = (worldTransform * vec4(position_in, 1.0)).xyz;
	gl_Position = viewProj * vec4(worldPos, 1.0);
}
