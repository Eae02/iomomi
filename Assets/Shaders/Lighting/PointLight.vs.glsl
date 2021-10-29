#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

#include <EGame.glh>

layout(location=0) in vec3 position_in;

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
	float invRange;
	float causticsScale;
	float causticsColorOffset;
	float causticsPanSpeed;
	float causticsTexScale;
	float shadowSampleDist;
	float specularIntensity;
} pc;

void main()
{
	vec3 worldPos = pc.position + position_in * pc.range;
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
