#version 450 core

#include "../Inc/DeferredLight.glh"
#include "../Inc/Light.glh"

layout(set=0, binding=4) uniform sampler2D brdfIntegrationMap;
layout(set=0, binding=5) uniform sampler2D reflectionMap;

const float DISTORT_INTENSITY = 0.1;

layout(push_constant) uniform PC
{
	vec3 normal;
	float offset;
} pc;

const float FULL_INTENSITY_DIST = 0.3;
const float ZERO_INTENSITY_DIST = 0.5;

vec3 CalculateLighting(GBData data)
{
	float distToPlane = abs(dot(data.worldPos, pc.normal) - pc.offset);
	float intensity = 1.0 - clamp((distToPlane - FULL_INTENSITY_DIST) / (ZERO_INTENSITY_DIST - FULL_INTENSITY_DIST), 0.0, 1.0);
	if (intensity < 0.0001)
		discard;
	
	vec3 toEye = renderSettings.cameraPosition - data.worldPos;
	vec3 dirToEye = normalize(toEye);
	vec3 reflSampleWS = data.worldPos;// - reflect(-toEye, data.normal) * DISTORT_INTENSITY;
	vec4 reflSamplePPS = renderSettings.viewProjection * vec4(reflSampleWS, 1.0);
	vec2 reflSampleSS = (reflSamplePPS.xy / reflSamplePPS.w) * 0.5 + 0.5;
	if (EG_VULKAN)
		reflSampleSS.y = 1.0 - reflSampleSS.y;
	
	vec3 fresnel = calcFresnel(data, dirToEye);
	vec2 envBRDF = texture(brdfIntegrationMap, vec2(max(dot(data.normal, dirToEye), 0.0), data.roughness)).rg;
	
	vec3 reflColor = texture(reflectionMap, reflSampleSS).rgb * (fresnel * envBRDF.x + envBRDF.y);
	
	return reflColor * intensity;
}
