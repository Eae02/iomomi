#version 450 core

layout(push_constant) uniform PC
{
	vec4 color;
};

layout(location=0) out vec4 color_out;

void main()
{
	color_out = color;
}
