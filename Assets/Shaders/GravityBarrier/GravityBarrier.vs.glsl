#version 450 core

layout(location=0) in vec2 locPos_in;

layout(location=0) out vec2 texCoord_out;
layout(location=1) out vec3 worldPos_out;

layout(push_constant) uniform PC
{
	vec3 position;
	float opacity;
	vec4 tangent;
	vec4 bitangent;
	uint blockedAxis;
	float noiseSampleOffset;
	float lineWidth;
	float glowDecay;
	float glowIntensity;
} pc;

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

void main()
{
	texCoord_out = locPos_in;
	
	vec2 rescaledPos = (locPos_in - vec2(0.5)) * vec2(pc.tangent.w, pc.bitangent.w);
	worldPos_out = pc.position + rescaledPos.x * pc.tangent.xyz + rescaledPos.y * pc.bitangent.xyz;
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
}
