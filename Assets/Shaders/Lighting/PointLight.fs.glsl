#version 450 core

#include "../Inc/Light.glh"
#include "../Inc/DeferredLight.glh"

layout(constant_id=0) const int softShadows = 1;

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
	float invRange;
} pc;

layout(location=1) noperspective in vec2 screenCoord_in;

layout(set=0, binding=4) uniform samplerCubeShadow shadowMap;

const vec3 SAMPLE_OFFSETS[] = vec3[]
(
	vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
	vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
	vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
	vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
	vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

const float SAMPLE_DIST = 0.005;
const float DEPTH_BIAS = 0.001;

vec3 CalculateLighting(GBData gbData)
{
	vec3 toLight = pc.position - gbData.worldPos;
	float dist = length(toLight);
	toLight = normalize(toLight);
	
	float compare = dist * pc.invRange - DEPTH_BIAS;
	
	float shadow = 0.0;
	if (softShadows != 0)
	{
		for (int i = 0; i < SAMPLE_OFFSETS.length(); i++)
		{
			vec3 samplePos = -toLight + SAMPLE_OFFSETS[i] * SAMPLE_DIST;
			shadow += texture(shadowMap, vec4(samplePos, compare)).r;
		}
		shadow /= float(SAMPLE_OFFSETS.length());
	}
	else
	{
		shadow = texture(shadowMap, vec4(-toLight, compare)).r;
	}
	
	if (shadow < 0.0001)
		discard;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	return calcDirectReflectance(toLight, dist, toEye, fresnel, gbData, pc.radiance * shadow);
}
