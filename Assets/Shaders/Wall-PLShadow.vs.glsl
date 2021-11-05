#version 450 core

#include <EGame.glh>

layout(location=0) in vec3 position_in;

layout(location=0) out vec3 worldPos_out;

layout(push_constant) uniform PC
{
	mat4 lightMatrix;
	vec4 lightSphere;
} pc;

void main()
{
	worldPos_out = position_in;
	gl_Position = pc.lightMatrix * vec4(position_in, 1.0);
}
