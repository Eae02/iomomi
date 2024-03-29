#version 450 core

layout(location=0) in vec3 position_in;

layout(location=0) out vec3 worldPos_out;

layout(push_constant) uniform PC
{
	vec3 switchPos;
	float intensity;
	mat3 rotationMatrix;
} pc;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	worldPos_out = pc.rotationMatrix * position_in + pc.switchPos;
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
}
