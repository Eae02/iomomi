#version 450 core

layout(set = 0, binding = 1) uniform Params_UseDynamicOffset
{
	vec4 color;
};

layout(location = 0) out vec4 color_out;

void main()
{
	color_out = color;
}
