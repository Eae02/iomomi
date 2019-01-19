#version 450 core

layout(location=0) in vec3 worldPos_out;
layout(location=1) in vec2 texCoord_out;
layout(location=2) in vec3 normal_out;
layout(location=3) in vec3 tangent_out;

layout(location=0) out vec4 color_out;

void main()
{
	color_out = vec4(1.0, 0.0, 0.0, 1.0);
}
