#version 450 core

#include "Inc/DeferredGeom.glh"
#include "Inc/RenderSettings.glh"

#include <EGame.glh>

layout(location=0) in vec3 worldPos_in;

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

layout(set=1, binding=0) uniform sampler2D reflectionMap;

const float ROUGHNESS = 0.2;
const float DISTORT_INTENSITY = 0.1;

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
	
	vec3 toEye = renderSettings.cameraPosition - worldPos_in;
	float distToEye = length(toEye);
	vec3 toEyeN = toEye / distToEye;
	
	vec3 reflSampleWS = worldPos_in - reflect(-toEye, normal) * DISTORT_INTENSITY;
	vec4 reflSamplePPS = renderSettings.viewProjection * vec4(reflSampleWS, 1.0);
	vec2 reflSampleSS = (reflSamplePPS.xy / reflSamplePPS.w) * 0.5 + 0.5;
	if (EG_VULKAN)
		reflSampleSS.y = 1.0 - reflSampleSS.y;
	
	vec3 waterColor = color * mix(minBrightness, 1.0, brightness);
	vec3 reflColor = texture(reflectionMap, reflSampleSS).rgb * color;
	
	DeferredOut(mix(waterColor, reflColor, 0.75), normal, ROUGHNESS, 1.0, AO_PRELIT);
}
