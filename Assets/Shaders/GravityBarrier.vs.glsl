#version 450 core

layout(location=0) in vec2 locPos_in;
layout(location=1) in vec4 worldPosOpacity_in;
layout(location=2) in vec4 tangent_in;
layout(location=3) in vec4 bitangent_in;

layout(location=0) out vec2 texCoord_out;
layout(location=1) out float opacity_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	texCoord_out = locPos_in * vec2(tangent_in.w, bitangent_in.w);
	vec3 worldPos = worldPosOpacity_in.xyz + (locPos_in.x - 0.5) * tangent_in.xyz + (locPos_in.y - 0.5) * bitangent_in.xyz;
	opacity_out = worldPosOpacity_in.w;
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
