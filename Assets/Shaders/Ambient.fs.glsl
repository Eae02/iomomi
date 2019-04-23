#version 450 core

#pragma variants VDefault VMSAA

#include "Inc/DeferredLight.glh"
#include "Inc/Light.glh"

struct LightProbe
{
	vec3 position;
	vec3 parallaxRad;
	vec3 inflInner;
	vec3 inflFalloff;
	int layer;
};

layout(set=1, binding=0) uniform ProbesBuffer
{
	LightProbe probes[8];
};

layout(set=1, binding=1) uniform sampler2D brdfIntegrationMap;
layout(set=1, binding=2) uniform samplerCubeArray irradianceMaps;
layout(set=1, binding=3) uniform samplerCubeArray spfMaps;

layout(push_constant) uniform PC
{
	vec3 constantAmbient;
};

vec3 CalculateLighting(GBData gb)
{
	vec3 irradiance = vec3(0.0);
	vec3 prefilteredColor = vec3(0.0);
	
	const float MAX_REFLECTION_LOD = 8.0;
	float pfLod = gb.roughness * MAX_REFLECTION_LOD;
	
	vec3 toEye = renderSettings.cameraPosition - gb.worldPos;
	vec3 dirToEye = normalize(toEye);
	vec3 R = reflect(-toEye, gb.normal);
	
	float totalWeight = 0.0;
	for (int i = 0; i < probes.length(); i++)
	{
		if (probes[i].layer == -1)
			continue;
		
		vec3 infl = (abs(gb.worldPos - probes[i].position) - probes[i].inflInner) * probes[i].inflFalloff;
		vec3 infl01 = clamp(infl, vec3(0), vec3(1));
		float weight = 1.0 - max(max(infl01.x, infl01.y), infl01.z);
		if (weight < 1E-6)
			continue;
		
		vec3 boxMax = probes[i].position + probes[i].parallaxRad;
		vec3 boxMin = probes[i].position - probes[i].parallaxRad;
		
		vec3 plane1Intersect = (boxMax - gb.worldPos) / R;
		vec3 plane2Intersect = (boxMin - gb.worldPos) / R;
		vec3 maxIntersect = max(plane1Intersect, plane2Intersect);
		float dist = min(min(maxIntersect.x, maxIntersect.y), maxIntersect.z);
		
		vec3 intersectWS = gb.worldPos + R * dist;
		vec4 sampleVec = vec4(intersectWS - probes[i].position, probes[i].layer);
		
		irradiance = texture(irradianceMaps, sampleVec).rgb * weight;
		prefilteredColor = textureLod(spfMaps, sampleVec, pfLod).rgb * weight;
		totalWeight += weight;
	}
	
	vec3 fresnel = calcFresnel(gb, dirToEye);
	vec3 kD = (vec3(1.0) - fresnel) * (1.0 - gb.metallic);
	vec3 diffuse = irradiance * gb.albedo;
	
	vec2 envBRDF = texture(brdfIntegrationMap, vec2(max(dot(gb.normal, dirToEye), 0.0), gb.roughness)).rg;
	vec3 specular = prefilteredColor * (fresnel * envBRDF.x + envBRDF.y);
	
	vec3 ambient = kD * diffuse + specular + gb.albedo * constantAmbient * max(1.0 - totalWeight, 0.0);
	
	return ambient * gb.ao;
}
