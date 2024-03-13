#version 450 core

#include <EGame.glh>

layout(location=0) in vec3 position_in;

layout(location=0) out vec3 worldPos_out;

layout(set=0, binding=0) uniform Parameters
{
	mat4 lightMatrix;
	vec4 lightSphere;
};

void main()
{
	worldPos_out = position_in;
	gl_Position = lightMatrix * vec4(position_in, 1.0);
}
