#version 450 core

#include "Inc/RenderSettings.glh"

layout(location=0) in vec3 position_in;

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
} pc;

void main()
{
	vec3 worldPos = pc.position + position_in * pc.range;
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
