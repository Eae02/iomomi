#version 450 core

#pragma variants VSoftShadows VHardShadows

#include "Inc/Light.glh"
#include "Inc/DeferredLight.glh"

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
	float invRange;
} pc;

layout(location=0) out vec4 color_out;

layout(location=1) noperspective in vec2 screenCoord_in;

layout(set=0, binding=4) uniform samplerCubeShadow shadowMap;

#ifdef VSoftShadows
const vec3 SAMPLE_OFFSETS[] = vec3[]
(
	vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
	vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
	vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
	vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
	vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);
#endif

const float SAMPLE_DIST = 0.005;
const float DEPTH_BIAS = 0.001;

void main()
{
	GBData gbData = ReadGB(screenCoord_in);
	
	vec3 toLight = pc.position - gbData.worldPos;
	float dist = length(toLight);
	
	float atten = calcAttenuation(dist);
	if (atten < 0.0001)
		discard;
	
	toLight /= dist;
	
	float compare = dist * pc.invRange - DEPTH_BIAS;
	
#ifdef VSoftShadows
	float shadow = 0.0;
	for (int i = 0; i < SAMPLE_OFFSETS.length(); i++)
	{
		vec3 samplePos = -toLight + SAMPLE_OFFSETS[i] * SAMPLE_DIST;
		shadow += texture(shadowMap, vec4(samplePos, compare)).r;
	}
	shadow /= float(SAMPLE_OFFSETS.length());
#endif
	
#ifdef VHardShadows
	float shadow = texture(shadowMap, vec4(-toLight, compare)).r;
#endif
	
	if (shadow < 0.0001)
		discard;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	color_out = vec4(calcDirectReflectance(toLight, toEye, fresnel, gbData, pc.radiance * (shadow * atten)), 0.0);
}
