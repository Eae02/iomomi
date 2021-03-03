#version 450 core

#include "Inc/DeferredGeom.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec4 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;
layout(location=3) in vec4 ao_in;
layout(location=4) in vec2 roughnessRange_in;

layout(binding=2) uniform sampler2DArray albedoSampler;
layout(binding=3) uniform sampler2DArray nmSampler;
layout(binding=4) uniform sampler2DArray mmSampler;

layout(push_constant) uniform PC
{
	float wallAOEnable;
};

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec4 miscMaps = texture(mmSampler, texCoord_in.xyz);
	
	vec3 normal = normalMapToWorld(texture(nmSampler, texCoord_in.xyz).xy, tbn);
	
	vec3 albedo = texture(albedoSampler, texCoord_in.xyz).rgb;
	
	vec2 ao2 = vec2(min(ao_in.x, ao_in.y), min(ao_in.w, ao_in.z));
	ao2 = pow(clamp(ao2, vec2(0.0), vec2(1.0)), vec2(0.5));
	float ao = miscMaps.b * mix(1, ao2.x * ao2.y, wallAOEnable);
	
	DeferredOut(albedo, normal, mix(roughnessRange_in.x, roughnessRange_in.y, miscMaps.r), miscMaps.g, ao);
}
