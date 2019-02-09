#version 450 core

layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

vec3 color1 = vec3(252, 207, 81) / 255.0;
vec3 color2 = color1 * 0.8;

void main()
{
	color_out = vec4(color1, 1.0);
}
