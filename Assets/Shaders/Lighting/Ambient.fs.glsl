#version 450 core

#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"
#include "../Inc/Light.glh"

layout(push_constant) uniform PC
{
	vec3 ambient;
};

layout(constant_id=0) const int ssrLinearSamples = 8;
layout(constant_id=1) const int ssrBinarySamples = 8;

vec3 ndc;
vec2 sampleTC;
bool behindDepthBuffer(vec3 worldPos)
{
	vec4 ndc3 = renderSettings.viewProjection * vec4(worldPos, 1.0);
	ndc = ndc3.xyz / ndc3.w;
	if (ndc.x < -1 || ndc.y < -1 || ndc.x > 1 || ndc.y > 1)
		return true;
	
	sampleTC = ndc.xy * 0.5 + 0.5;
	if (!EG_OPENGL)
		sampleTC.y = 1 - sampleTC.y;
	
	return depthTo01(ndc.z) > texture(gbDepthSampler, sampleTC).r;
}

vec3 calcReflection(vec3 surfacePos, vec3 dirToEye, vec3 normal)
{
	if (ssrLinearSamples == 0)
		return ambient;
	
	surfacePos += normal * 0.01;
	
	const float MAX_DIST   = 5;   //Maximum distance to reflection
	const float FADE_BEGIN = 0.75; //Percentage of screen radius to begin fading out at
	
	vec3 rayDir = normalize(reflect(-dirToEye, normal)) * (MAX_DIST / ssrLinearSamples);
	
	for (uint i = 1; i <= ssrLinearSamples; i++)
	{
		vec3 sampleWorldPos = surfacePos + rayDir * i;
		if (behindDepthBuffer(sampleWorldPos))
		{
			float lo = i - 1;
			float hi = i;
			for (int i = 0; i < ssrBinarySamples; i++)
			{
				float mid = (lo + hi) / 2.0;
				if (behindDepthBuffer(surfacePos + rayDir * mid))
					hi = mid;
				else
					lo = mid;
			}
			
			float fade01 = max(abs(ndc.x), abs(ndc.y));
			float fade = clamp((fade01 - 1) / (1 - FADE_BEGIN) + 1, 0, 1) * (length(rayDir * lo) / MAX_DIST);
			return mix(texture(gbColor1Sampler, sampleTC).rgb, ambient, fade);
		}
	}
	
	return ambient;
}

vec3 CalculateLighting(GBData gbData)
{
	if (gbData.ao > 0.99)
		return gbData.albedo;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	vec3 kd = (1.0 - fresnel) * (1.0 - gbData.metallic);
	
	vec3 reflection = calcReflection(gbData.worldPos, toEye, gbData.normal) * (1 - gbData.roughness);
	return (gbData.albedo * kd * ambient + reflection * fresnel) * gbData.ao;
}
