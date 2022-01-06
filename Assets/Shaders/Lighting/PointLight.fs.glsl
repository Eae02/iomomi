#version 450 core

#include "../Inc/Light.glh"
#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"

const int SHADOW_MODE_NONE = 0;
const int SHADOW_MODE_HARD = 1;
const int SHADOW_MODE_SOFT = 2;

layout(constant_id=0) const int shadowMode = 0;
layout(constant_id=1) const int enableCaustics = 0;

layout(push_constant, std140) uniform PC
{
	vec4 positionAndRange;
	vec4 radiance;
	float causticsScale;
	float causticsColorOffset;
	float causticsPanSpeed;
	float causticsTexScale;
	float shadowSampleDist;
	float specularIntensity;
} pc;

layout(set=0, binding=4) uniform sampler2D waterDepth;

layout(set=0, binding=5) uniform sampler3D causticsTexture;

layout(set=0, binding=6) uniform samplerCubeShadow shadowMap;

const vec3 SAMPLE_OFFSETS[] = vec3[]
(
	vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
	vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1)
);

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
	vec3 toLight = pc.positionAndRange.xyz - gbData.worldPos;
	float dist = length(toLight);
	toLight = normalize(toLight);
	
	vec3 radiance = pc.radiance.xyz;
	
	if (shadowMode != SHADOW_MODE_NONE)
	{
		float compare = dist / pc.positionAndRange.w;
		float shadow = 0.0;
		if (shadowMode == SHADOW_MODE_SOFT)
		{
			for (int i = 0; i < SAMPLE_OFFSETS.length(); i++)
			{
				vec3 samplePos = -toLight + SAMPLE_OFFSETS[i] * pc.shadowSampleDist;
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
		radiance *= shadow;
	}
	
	if (enableCaustics == 1)
	{
		vec4 waterDepth4 = texture(waterDepth, vec2(gl_FragCoord.xy) / vec2(textureSize(waterDepth, 0).xy));
		float lDepth = linearizeDepth(gbData.hDepth);
		float maxWaterDepth = waterDepth4.g + (waterDepth4.r < 0.5 ? 0.2 : 0.4);
		if (lDepth > waterDepth4.r + 0.2 && lDepth < maxWaterDepth)
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
	}
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	return calcDirectReflectance(toLight, dist, toEye, fresnel, gbData, radiance, pc.specularIntensity);
}
