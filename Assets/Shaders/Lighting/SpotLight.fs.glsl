#version 450 core

#include "../Inc/Light.glh"
#include "../Inc/DeferredLight.glh"

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 direction;
	float penumbraBias;
	vec3 directionL;
	float penumbraScale;
	vec3 radiance;
	float width;
} pc;

vec3 CalculateLighting(GBData gbData)
{
	if ((gbData.flags & RF_NO_LIGHTING) != 0)
		discard;
	
	vec3 toLight = pc.position - gbData.worldPos;
	float dist = length(toLight);
	toLight /= dist;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	float cosT = dot(toLight, -pc.direction);
	float penumbraFactor = min((cosT + pc.penumbraBias) * pc.penumbraScale, 1.0);
	if (penumbraFactor <= 0.0)
		discard;
	
	return calcDirectReflectance(toLight, dist, toEye, fresnel, gbData, pc.radiance * penumbraFactor);
}
