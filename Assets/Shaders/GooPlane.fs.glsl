#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/Fresnel.glh"
#include <Deferred.glh>

#include <EGame.glh>

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec4 edgeDists_in;

layout(location=0) out vec4 color_out;

const int NM_SAMPLES = 3;

layout(set=0, binding=1, std140) uniform NMTransformsUB
{
	vec3 color;
	float minBrightness;
	vec4 nmTransforms[NM_SAMPLES * 2];
};

layout(set=0, binding=2) uniform sampler2D normalMap;

const float ROUGHNESS = 0.2;
const float DISTORT_INTENSITY = 0.1;

const float EDGE_FADE_DIST = 0.3;

void main()
{
	float edgeDist = max(max(edgeDists_in.x, edgeDists_in.y), max(edgeDists_in.z, edgeDists_in.w));
	float edgeFade = clamp(edgeDist / EDGE_FADE_DIST + 1.0 - 1.0 / EDGE_FADE_DIST, 0, 1);
	
	const float VISIBILITY_DEPTH = 1.5;
	
	float depth = 100;
	float alpha = mix(0.1, 1.0, clamp(depth / VISIBILITY_DEPTH, 0, 1));
	
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
	
	const float MAX_BRIGHTNESS = 12.0;
	const float CUTOFF = 0.45;
	vec3 waterColor = color * mix(minBrightness, 1.0, clamp(brightness + edgeFade, 0, 1));
	
	float glow = max(brightness + edgeFade * 0.3 - CUTOFF, 0.0) * MAX_BRIGHTNESS / (1.0 - CUTOFF);
	waterColor += color * glow;
	
	color_out = vec4(mix(waterColor, color * minBrightness, 0), alpha);
}
