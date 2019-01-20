#version 450 core

layout(location=0) in vec3 worldPos_out;
layout(location=1) in vec2 texCoord_out;
layout(location=2) in vec3 normal_out;
layout(location=3) in vec3 tangent_out;

layout(location=0) out vec4 color_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

vec3 color1 = vec3(252, 207, 81) / 255.0;
vec3 color2 = color1 * 0.8;

void main()
{
	float x = pow(sin(abs(texCoord_out.y - 0.5) * 8.0 - renderSettings.gameTime * 5.0), 2.0);
	color_out = vec4(mix(color1, color2, x), 1.0);
}
