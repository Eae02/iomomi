#version 450 core

#include "../Inc/Light.glh"
#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"

layout(constant_id=0) const int softShadows = 1;

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
	float invRange;
	float causticsScale;
	float causticsColorOffset;
	float causticsPanSpeed;
	float causticsTexScale;
} pc;

layout(location=1) noperspective in vec2 screenCoord_in;

layout(set=0, binding=5) uniform sampler2D waterDepth;

layout(set=0, binding=6) uniform sampler3D causticsTexture;

layout(set=0, binding=7) uniform samplerCubeShadow shadowMap;

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

float sampleCaustics(vec2 pos)
{
	pos *= pc.causticsTexScale;
	float causticsZ = renderSettings.gameTime * 0.2;
	float panOffset = renderSettings.gameTime * pc.causticsPanSpeed;
	float c1 = texture(causticsTexture, vec3(pos.x + panOffset, pos.y, causticsZ)).r;
	float c2 = texture(causticsTexture, vec3((pos.x - panOffset) * 0.8, (pos.y + panOffset * 0.2) * 0.8, causticsZ + 0.5)).r;
	return min(c1, c2);
}

vec3 CalculateLighting(GBData gbData)
{
	if ((gbData.flags & RF_NO_LIGHTING) != 0)
		discard;
	
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
	
	vec3 radiance = pc.radiance * shadow;
	vec4 waterDepth3 = texture(waterDepth, screenCoord_in);
	float lDepth = linearizeDepth(gbData.hDepth);
	if (lDepth > waterDepth3.r && lDepth < waterDepth3.a + 0.3)
	{
		vec2 offsetG = vec2(0.01, 0.003) * pc.causticsColorOffset;
		vec2 offsetB = vec2(0, -0.015) * pc.causticsColorOffset;
		
		vec3 caustics = vec3(0);
		float weightSum = 0;
		for (int p = 0; p < 3; p++)
		{
			float w = abs(gbData.normal[p]);
			vec2 samplePos = vec2(gbData.worldPos[(p + 1) % 3], gbData.worldPos[(p + 2) % 3]);
			caustics += vec3(
				sampleCaustics(samplePos),
				sampleCaustics(samplePos + offsetG),
				sampleCaustics(samplePos + offsetB)) * w;
			weightSum += w;
		}
		caustics /= weightSum;
		radiance *= vec3(1) + caustics * pc.causticsScale;
	}
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	return calcDirectReflectance(toLight, dist, toEye, fresnel, gbData, radiance);
}
