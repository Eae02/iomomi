#version 450 core

layout(location=0) in vec4 color_in;

layout(location=0) out vec4 color0_out;
layout(location=1) out vec4 color1_out;

void main()
{
	color0_out = vec4(color_in.rgb, 0);
	color1_out = vec4(0, 0, 0, 0);
}
