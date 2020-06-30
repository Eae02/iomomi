#version 450 core

#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"
#include "../Inc/Light.glh"

layout(push_constant) uniform PC
{
	vec3 ambient;
};

vec3 CalculateLighting(GBData gbData)
{
	if ((gbData.flags & RF_NO_LIGHTING) != 0)
		return gbData.albedo;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	vec3 kd = (1.0 - fresnel) * (1.0 - gbData.metallic);
	
	return (gbData.albedo * kd + fresnel * (1 - gbData.roughness)) * ambient * gbData.ao;
}
