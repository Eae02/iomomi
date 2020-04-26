#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

#include <EGame.glh>

layout(location=0) in vec3 position_in;
layout(location=1) noperspective out vec2 screenCoord_out;

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
	float invRange;
} pc;

void main()
{
	vec3 worldPos = pc.position + position_in * pc.range;
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
	screenCoord_out = (gl_Position.xy / gl_Position.z) * 0.5 + 0.5;
	if (EG_VULKAN)
	{
		screenCoord_out.y = 1.0 - screenCoord_out.y;
	}
}
