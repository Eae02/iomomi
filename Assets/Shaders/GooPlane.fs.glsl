#version 450 core

#include "Inc/DeferredGeom.glh"

layout(location=0) in vec3 worldPos_in;

#include "Inc/RenderSettings.glh"

layout(set=0, binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

const int NM_SAMPLES = 3;

layout(set=0, binding=1, std140) uniform NMTransformsUB
{
	vec3 color;
	float minBrightness;
	vec4 nmTransforms[NM_SAMPLES * 2];
};

layout(set=0, binding=2) uniform sampler2D normalMap;

void main()
{
	vec3 normal = vec3(0.0);
	float brightness = 0.0;
	for (int i = 0; i < nmTransforms.length(); i += 2)
	{
		vec4 w4 = vec4(worldPos_in, renderSettings.gameTime);
		vec2 samplePos = vec2(dot(nmTransforms[i], w4), dot(nmTransforms[i + 1], w4));
		vec4 nmValue = texture(normalMap, samplePos);
		normal += (nmValue.rbg * (255.0 / 128.0)) - vec3(1.0);
		brightness += nmValue.a;
	}
	normal = normalize(normal);
	brightness /= NM_SAMPLES;
	
	DeferredOut(color * mix(minBrightness, 1.0, brightness), normal, 0.25, 1.0, 1.0);
}
