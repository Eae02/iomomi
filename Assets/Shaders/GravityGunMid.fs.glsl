#version 450 core

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

const vec3 COLOR = vec3(0.12, 0.9, 0.7);

layout(binding=1) uniform sampler2D hexSampler;

void main()
{
	float lDist = texCoord_in.y;
	
	float hex = texture(hexSampler, texCoord_in * 4).r;
	float edge = max(max(abs(texCoord_in.x / 1.6 - 0.5), abs(texCoord_in.y - 0.5)) * 4.0 - 1.0, 0.0);
	
	float intensity = mix(hex, 1.0, (sin(lDist * 15 + renderSettings.gameTime * 2) * 0.5 + 0.5) * 0.5);
	
	intensity += mix(0.1, 1.0, pow(edge, 3));
	
	color_out = vec4(COLOR * mix(0.1, 1.0, intensity), 1.0);
}
