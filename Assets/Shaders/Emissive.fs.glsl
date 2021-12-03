#version 450 core

layout(location=0) out vec4 color_out;

layout(location=0) in vec4 color_in;

layout(push_constant) uniform PC
{
	float alpha;
};

void main()
{
	color_out = vec4(color_in.rgb, alpha);
}
