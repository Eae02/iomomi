#version 450 core

#include "Inc/DeferredGeom.glh"

layout(location=0) in vec3 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;

layout(binding=1) uniform sampler2DArray albedoSampler;
layout(binding=2) uniform sampler2DArray nmSampler;
layout(binding=3) uniform sampler2DArray mmSampler;

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec4 miscMaps = texture(mmSampler, texCoord_in);
	
	vec3 nmNormal = (texture(nmSampler, texCoord_in).grb * (255.0 / 128.0)) - vec3(1.0);
	vec3 normal = normalize(tbn * nmNormal);
	
	vec3 albedo = texture(albedoSampler, texCoord_in).rgb;
	
	DeferredOut(albedo, normal, mix(0.5, 1.0, miscMaps.r), miscMaps.g, miscMaps.b);
}
