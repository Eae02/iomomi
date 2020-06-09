#version 450 core

layout(push_constant) uniform PC
{
	mat4 viewProj;
	uint flags;
};

layout(location=0) out uint flags_out;

void main()
{
	flags_out = flags;
}
