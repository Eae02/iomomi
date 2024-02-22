#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/Utils.glh"

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

const vec3 COLOR = vec3(0.12, 0.9, 0.7);

layout(binding=1) uniform sampler2D hexSampler;

layout(push_constant) uniform PC
{
	layout(offset=112) float intensityBoost;
};

void main()
{
	vec2 hex = texture(hexSampler, texCoord_in * 4).rg;
	float edge = max(max(abs(texCoord_in.x / 1.6 - 0.5), abs(texCoord_in.y - 0.5)) * 4.0 - 1.0, 0.0);
	
	float intensity = hex.r * sin01(hex.g * TWO_PI + renderSettings.gameTime * 2) + intensityBoost;
	
	intensity += mix(0.1, 1.0, pow(edge, 3));
	
	color_out = vec4(COLOR * (0.1 + intensity), 1.0);
}
