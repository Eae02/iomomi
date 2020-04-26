#version 450 core

#pragma variants VDefault VNoRefl

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/Fresnel.glh"
#include <Deferred.glh>

#include <EGame.glh>

layout(location=0) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

const int NM_SAMPLES = 3;

layout(set=0, binding=1, std140) uniform NMTransformsUB
{
	vec3 color;
	float minBrightness;
	vec4 nmTransforms[NM_SAMPLES * 2];
};

layout(set=0, binding=2) uniform sampler2D normalMap;

layout(set=1, binding=0) uniform sampler2D depthBuffer;
layout(set=1, binding=1) uniform sampler2D reflectionMap;

const float ROUGHNESS = 0.2;
const float DISTORT_INTENSITY = 0.1;

void main()
{
	const float VISIBILITY_DEPTH = 1.5;
	
	vec2 screenCoord = vec2(gl_FragCoord.xy) / vec2(textureSize(depthBuffer, 0).xy);
	float hDepth = texture(depthBuffer, screenCoord).r;
	vec3 geomWorldPos = WorldPosFromDepth(hDepth, screenCoord, renderSettings.invViewProjection);
	
	float depth = distance(worldPos_in, geomWorldPos);
	float alpha = mix(0.1, 1.0, depth / VISIBILITY_DEPTH);
	
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
	
	const float MAX_BRIGHTNESS = 15.0;
	const float CUTOFF = 0.45;
	vec3 waterColor = color * mix(minBrightness, 1.0, brightness);
	
#ifndef VNoRefl
	vec3 toEye = normalize(renderSettings.cameraPosition - worldPos_in);
	vec3 reflSampleWS = worldPos_in - reflect(-toEye, normal) * DISTORT_INTENSITY;
	vec4 reflSamplePPS = renderSettings.viewProjection * vec4(reflSampleWS, 1.0);
	vec2 reflSampleSS = (reflSamplePPS.xy / reflSamplePPS.w) * 0.5 + 0.5;
	if (EG_VULKAN)
		reflSampleSS.y = 1.0 - reflSampleSS.y;
	
	vec3 F0 = vec3(0.04);
	vec3 fresnel = pow(fresnelSchlick(max(dot(normal, toEye), 0.0), F0, ROUGHNESS), vec3(2.5));
	
	vec3 reflColor = textureLod(reflectionMap, reflSampleSS, 0).rgb * color;
	waterColor = mix(waterColor, reflColor, fresnel);
#endif
	
	float glow = max(brightness - CUTOFF, 0.0) * MAX_BRIGHTNESS / (1.0 - CUTOFF);
	alpha += glow;
	waterColor += color * glow;
	
	color_out = vec4(waterColor, min(alpha, 1.0));
}
