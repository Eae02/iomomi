#version 450 core

#include "Inc/DeferredGeom.glh"

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(binding=1) uniform sampler2D albedoSampler;
layout(binding=2) uniform sampler2D nmSampler;
layout(binding=3) uniform sampler2D mmSampler;

layout(push_constant) uniform PC
{
	vec2 roughnessRange;
	vec2 textureScale;
};

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec2 texCoord = textureScale * texCoord_in;
	vec4 miscMaps = texture(mmSampler, texCoord);
	
	vec3 nmNormal = (texture(nmSampler, texCoord).grb * (255.0 / 128.0)) - vec3(1.0);
	vec3 normal = normalize(tbn * nmNormal);
	
	vec3 albedo = texture(albedoSampler, texCoord).rgb;
	
	float roughness = mix(roughnessRange.x, roughnessRange.y, miscMaps.r);
	DeferredOut(albedo, normal, roughness, miscMaps.g, miscMaps.b);
}
