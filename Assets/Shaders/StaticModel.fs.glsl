#version 450 core

#pragma variants VGameTex2D VGameTexArray VEditorTex2D VEditorTexArray

layout(constant_id=5) const int alphaTest = 0;

#if defined(VEditorTex2D) || defined(VEditorTexArray)
#define EDITOR
#include "Inc/EditorLight.glh"
layout(location=0) out vec4 color_out;
#else
#include "Inc/DeferredGeom.glh"
#endif

#include "Inc/NormalMap.glh"

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

#if defined(VGameTexArray) || defined(VEditorTexArray)
#define TEXTURE_TYPE sampler2DArray
#else
#define TEXTURE_TYPE sampler2D
#endif

layout(binding=1) uniform TEXTURE_TYPE albedoSampler;
layout(binding=2) uniform TEXTURE_TYPE nmSampler;
layout(binding=3) uniform TEXTURE_TYPE mmSampler;

layout(push_constant) uniform PC
{
	vec2 roughnessRange;
	vec2 textureScale;
#if defined(VGameTexArray) || defined(VEditorTexArray)
	float textureLayer;
#endif
};

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
#if defined(VGameTexArray) || defined(VEditorTexArray)
	vec3 texCoord = vec3(textureScale * texCoord_in, textureLayer);
#else
	vec2 texCoord = textureScale * texCoord_in;
#endif
	
	vec4 miscMaps = texture(mmSampler, texCoord);
	vec3 normal = normalMapToWorld(texture(nmSampler, texCoord).xy, tbn);
	vec4 albedo = texture(albedoSampler, texCoord);
	
	if (alphaTest == 1 && albedo.a < 0.5)
		discard;
	
#ifdef EDITOR
	color_out = vec4(albedo.rgb * CalcEditorLight(normal, miscMaps.b), 1.0);
#else
	float roughness = mix(roughnessRange.x, roughnessRange.y, miscMaps.r);
	DeferredOut(albedo.rgb / albedo.a, normal, roughness, miscMaps.g, miscMaps.b);
#endif
}
