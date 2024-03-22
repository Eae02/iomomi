#version 450 core

#include "Inc/DeferredGeom.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec3 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;
layout(location=3) in vec2 roughnessRange_in;

layout(binding=1) uniform texture2DArray albedoTex;
layout(binding=2) uniform texture2DArray nmTex;
layout(binding=3) uniform texture2DArray mmTex;

layout(binding=4) uniform sampler uSampler;

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec4 miscMaps = texture(sampler2DArray(mmTex, uSampler), texCoord_in);
	
	vec3 normal = normalMapToWorld(texture(sampler2DArray(nmTex, uSampler), texCoord_in).xy, tbn);
	
	vec3 albedo = texture(sampler2DArray(albedoTex, uSampler), texCoord_in).rgb;
	
	DeferredOut(albedo, normal, mix(roughnessRange_in.x, roughnessRange_in.y, miscMaps.r), miscMaps.g, miscMaps.b);
}
