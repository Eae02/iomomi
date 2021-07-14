#version 450 core

layout(location=0) out vec4 color_out;

layout(location=0) in vec4 color_in;

void main()
{
	color_out = color_in;
}
