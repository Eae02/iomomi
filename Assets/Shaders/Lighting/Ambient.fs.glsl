#version 450 core

#pragma variants VSSAO VNoSSAO

#define HDEPTH_TO_ALPHA
#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"
#include "../Inc/Light.glh"

#ifdef VSSAO
layout(set=0, binding=5) uniform sampler2D ssaoSampler;
#endif

layout(push_constant) uniform PC
{
	vec3 ambient;
};

vec3 CalculateLighting(GBData gbData)
{
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	vec3 kd = (1.0 - fresnel) * (1.0 - gbData.metallic);
	
	float ao = gbData.ao;
#ifdef VSSAO
	ao *= texelFetch(ssaoSampler, ivec2(gl_FragCoord.xy), 0).r;
#endif
	
	return (gbData.albedo * kd + fresnel * (1 - gbData.roughness)) * ambient * ao;
}
