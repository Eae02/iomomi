#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(set = 0, binding = 4) uniform Params
{
	vec3 actPos;
	float actIntensity;
};

const float PI = 3.14159265;

layout(location = 0) in vec3 worldPos_in;
layout(location = 1) in vec2 texCoord_in;

layout(location = 0) out vec4 color_out;

layout(binding = 1) uniform texture2D lightDistTex;
layout(binding = 2) uniform texture2D hexTex;
layout(binding = 3) uniform sampler commonSampler;

const vec3 COLOR = vec3(0.12, 0.9, 0.7);

void main()
{
	float lDist = texture(sampler2D(lightDistTex, commonSampler), texCoord_in).r;
	float hex = texture(sampler2D(hexTex, commonSampler), texCoord_in * 4).r;
	if (lDist < 0.001)
		discard;

	float intensity = hex * (sin(lDist * 10 + renderSettings.gameTime * 5) * 0.5 + 0.5) * 0.75;

	float actDist = distance(actPos, worldPos_in);
	intensity += actIntensity / (actDist * 10.0 + 1.0);

	intensity += mix(0.1, 1.0, pow(1.0 - lDist, 3));

	color_out = vec4(COLOR * mix(0.1, 1.5, intensity), 1.0);
}
