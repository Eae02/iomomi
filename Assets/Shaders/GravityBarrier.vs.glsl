#version 450 core

layout(location=0) in vec2 locPos_in;

layout(location=0) out vec2 texCoord_out;
layout(location=1) out vec3 worldPos_out;
layout(location=2) out vec2 ypc_out;

layout(push_constant) uniform PC
{
	vec3 position;
	float opacity;
	vec4 tangent;
	vec4 bitangent;
	uint blockedAxis;
} pc;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	texCoord_out = locPos_in;
	worldPos_out = pc.position + (locPos_in.x - 0.5) * pc.tangent.xyz + (locPos_in.y - 0.5) * pc.bitangent.xyz;
	ypc_out = vec2((locPos_in.y - 0.5) * pc.bitangent.w, pc.bitangent.w * 0.5);
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
}
