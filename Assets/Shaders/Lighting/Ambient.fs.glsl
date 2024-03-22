#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

#pragma variants VSSAO VNoSSAO

#include "../Inc/DeferredLight.glh"
#include "../Inc/Depth.glh"
#include "../Inc/Light.glh"

layout(set=0, binding=4) uniform ParamsUB_UseDynamicOffset
{
	vec3 ambient;
	float onlyAO;
};

#ifdef VSSAO
layout(set=1, binding=0) uniform texture2D ssaoTexture;
#endif

vec3 CalculateLighting(GBData gbData)
{
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	vec3 fresnel = calcFresnel(gbData, toEye);
	vec3 kd = (1.0 - fresnel) * (1.0 - gbData.metallic);
	
	float ao = gbData.ao;
#ifdef VSSAO
	ao *= texelFetch(ssaoTexture, ivec2(gl_FragCoord.xy), 0).r;
#endif
	return max((gbData.albedo * kd + fresnel * (1 - gbData.roughness)) * ambient, vec3(onlyAO)) * ao;
}
