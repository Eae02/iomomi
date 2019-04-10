#version 450 core

#include "Inc/RenderSettings.glh"

layout(location=0) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

layout(set=0, binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

const int NM_SAMPLES = 3;

layout(set=0, binding=1, std140) uniform NMTransformsUB
{
	vec3 color;
	vec4 nmTransforms[NM_SAMPLES * 2];
};

layout(set=0, binding=2) uniform sampler2D normalMap;

const float ROUGHNESS = 0.2;

void main()
{
	float brightness = 0.0;
	for (int i = 0; i < nmTransforms.length(); i += 2)
	{
		vec4 w4 = vec4(worldPos_in, renderSettings.gameTime);
		vec2 samplePos = vec2(dot(nmTransforms[i], w4), dot(nmTransforms[i + 1], w4));
		brightness += texture(normalMap, samplePos).a;
	}
	brightness /= NM_SAMPLES;
	
	const float MAX_BRIGHTNESS = 10.0;
	const float CUTOFF = 0.45;
	
	color_out = vec4(color * ((max(brightness - CUTOFF, 0.0) * MAX_BRIGHTNESS / (1.0 - CUTOFF))), 0.0);
}
