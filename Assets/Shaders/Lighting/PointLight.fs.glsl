#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

#include "../Inc/Light.glh"
#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"

layout(constant_id=0) const int softShadows = 0;
layout(constant_id=1) const int enableCaustics = 0;

layout(set=0, binding=4) uniform texture2D waterDepth;
layout(set=0, binding=5) uniform texture3D causticsTexture;
layout(set=0, binding=6) uniform sampler causticsSampler;
layout(set=0, binding=7) uniform samplerShadow shadowMapSampler_ReflShadow;

layout(set=1, binding=0) uniform ParamsUB_UseDynamicOffset
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

layout(set=2, binding=0) uniform textureCube shadowMap;

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
	float c1 = textureLod(sampler3D(causticsTexture, causticsSampler), vec3(pos.x + panOffset, pos.y, causticsZ), 0).r;
	float c2 = textureLod(sampler3D(causticsTexture, causticsSampler), vec3((pos.x - panOffset) * 0.8, (pos.y + panOffset * 0.2) * 0.8, causticsZ + 0.5), 0).r;
	return min(c1, c2);
}

vec3 CalculateLighting(GBData gbData)
{
	vec3 toLight = pc.positionAndRange.xyz - gbData.worldPos;
	float dist = length(toLight);
	toLight = normalize(toLight);
	
	vec3 radiance = pc.radiance.xyz;
	
	float compare = dist / pc.positionAndRange.w;
	float shadow = 0.0;
	if (bool(softShadows))
	{
		for (int i = 0; i < SAMPLE_OFFSETS.length(); i++)
		{
			vec3 samplePos = -toLight + SAMPLE_OFFSETS[i] * pc.shadowSampleDist;
			shadow += texture(samplerCubeShadow(shadowMap, shadowMapSampler_ReflShadow), vec4(samplePos, compare)).r;
		}
		shadow /= float(SAMPLE_OFFSETS.length());
	}
	else
	{
		shadow = texture(samplerCubeShadow(shadowMap, shadowMapSampler_ReflShadow), vec4(-toLight, compare)).r;
	}
	if (shadow < 0.0001)
		discard;
	radiance *= shadow;
	
	if (bool(enableCaustics))
	{
		vec4 waterDepth4 = texelFetch(waterDepth, ivec2(min(gl_FragCoord.xy, textureSize(waterDepth, 0) - 1)), 0);
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
