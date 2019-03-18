#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in mat4 worldTransform_in;

layout(location=0) out vec3 worldPos_out;

void main()
{
	worldPos_out = (worldTransform_in * vec4(position_in, 1.0)).xyz;
}
