#version 450 core

layout(location=0) in vec2 locPos_in;
layout(location=1) in vec3 worldPos_in;
layout(location=2) in uvec2 opacityBlockedAxis_in;
layout(location=3) in vec4 tangent_in;
layout(location=4) in vec4 bitangent_in;

layout(location=0) out vec2 texCoord_out;
layout(location=1) out vec3 worldPos_out;
layout(location=2) out vec3 tangent_out;
layout(location=3) out vec2 ypc_out;
layout(location=4) out float opacity_out;
layout(location=5) flat out uint blockedAxis_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

const float OPACITY_SCALE = 0.0004 / 255.0;

void main()
{
	texCoord_out = locPos_in * vec2(tangent_in.w, bitangent_in.w);
	worldPos_out = worldPos_in.xyz + (locPos_in.x - 0.5) * tangent_in.xyz + (locPos_in.y - 0.5) * bitangent_in.xyz;
	opacity_out = opacityBlockedAxis_in.x * OPACITY_SCALE;
	tangent_out = tangent_in.xyz;
	ypc_out = vec2((locPos_in.y - 0.5) * bitangent_in.w, bitangent_in.w * 0.5);
	blockedAxis_out = opacityBlockedAxis_in.y;
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
}
