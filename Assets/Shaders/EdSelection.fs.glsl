#version 450 core

layout(location=0) out float color_out;

layout(push_constant) uniform PC
{
	mat4 transform;
	float intensity;
};

void main()
{
	color_out = intensity;
}
