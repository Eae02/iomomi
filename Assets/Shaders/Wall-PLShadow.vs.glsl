#version 450 core

layout(location=0) in vec3 position_in;

layout(location=0) out vec3 worldPos_out;

void main()
{
	worldPos_out = position_in;
}
