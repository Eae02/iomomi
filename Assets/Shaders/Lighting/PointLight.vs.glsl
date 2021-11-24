#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

#include <EGame.glh>

layout(location=0) in vec3 position_in;

layout(push_constant, std140) uniform PC
{
	vec4 positionAndRange;
	vec4 radiance;
	float causticsScale;
	float causticsColorOffset;
	float causticsPanSpeed;
	float causticsTexScale;
	float shadowSampleDist;
	float specularIntensity;
} pc;

void main()
{
	vec3 worldPos = pc.positionAndRange.xyz + position_in * pc.positionAndRange.w;
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
