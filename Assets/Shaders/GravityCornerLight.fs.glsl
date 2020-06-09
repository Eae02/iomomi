#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(push_constant) uniform PC
{
	vec3 actPos;
	float actIntensity;
} pc;

const float PI = 3.14159265;

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D lightDistSampler;
layout(binding=2) uniform sampler2D hexSampler;

const vec3 COLOR = vec3(0.12, 0.9, 0.7);

void main()
{
	float lDist = texture(lightDistSampler, texCoord_in).r;
	if (lDist < 0.001)
		discard;
	
	float hex = texture(hexSampler, texCoord_in * 4).r;
	
	float intensity = hex * (sin(lDist * 10 + renderSettings.gameTime * 5) * 0.5 + 0.5) * 0.75;
	
	float actDist = distance(pc.actPos, worldPos_in);
	intensity += pc.actIntensity / (actDist * 10.0 + 1.0);
	
	intensity += mix(0.1, 1.0, pow(1.0 - lDist, 3));
	
	color_out = vec4(COLOR * mix(0.1, 1.5, intensity), 1.0);
}
