#version 450 core

#pragma variants VDefault VEditor

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color0_out;

#ifdef VEditor
#include "Inc/EditorLight.glh"
#endif

#ifdef VDefault
layout(location=1) out vec4 color1_out;
#endif

#include <Deferred.glh>

#include "Inc/NormalMap.glh"

layout(binding=1) uniform texture2D albedoSampler;

layout(binding=2) uniform texture2D normalMapSampler;

layout(binding=3) uniform sampler samplerCommon;

layout(binding=4) uniform ParamsUB
{
	float roughness;
	float globalAlpha;
};

layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec3 normal = normalMapToWorld(texture(sampler2D(normalMapSampler, samplerCommon), texCoord_in).xy, tbn);
	
	vec4 albedo = texture(sampler2D(albedoSampler, samplerCommon), texCoord_in);
	float opacity = albedo.a * globalAlpha;
	
#ifdef VEditor
	color0_out = vec4(albedo.rgb * CalcEditorLight(normal, 1.0), opacity);
#endif
	
#ifdef VDefault
	color0_out = vec4(albedo.rgb, opacity);
	color1_out = vec4(SMEncode(normal), roughness, opacity);
#endif
}
