#version 450 core

#pragma variants VGameTex2D VGameTexArray VEditorTex2D VEditorTexArray

layout(constant_id = 5) const int alphaTest = 0;

#if defined(VEditorTex2D) || defined(VEditorTexArray)
#define EDITOR
#include "Inc/EditorLight.glh"
layout(location = 0) out vec4 color_out;
#else
#include "Inc/DeferredGeom.glh"
#endif

#include "Inc/NormalMap.glh"

layout(location = 0) in vec3 worldPos_in;
layout(location = 1) in vec2 texCoord_in;
layout(location = 2) in vec3 normal_in;
layout(location = 3) in vec3 tangent_in;

#if defined(VGameTexArray) || defined(VEditorTexArray)
#define TEXTURE_TYPE texture2DArray
#define SAMPLER(tex) sampler2DArray(tex, uSampler)
#else
#define TEXTURE_TYPE texture2D
#define SAMPLER(tex) sampler2D(tex, uSampler)
#endif

layout(binding = 1) uniform TEXTURE_TYPE albedoTex;
layout(binding = 2) uniform TEXTURE_TYPE normalMapTex;
layout(binding = 3) uniform TEXTURE_TYPE miscMapTex;

layout(binding = 4) uniform sampler uSampler;

layout(binding = 5) uniform ParametersUB
{
	float roughnessMin;
	float roughnessMax;
	vec2 textureScale;
	float textureLayer;
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

	vec4 miscMaps = texture(SAMPLER(miscMapTex), texCoord);
	vec3 normal = normalMapToWorld(texture(SAMPLER(normalMapTex), texCoord).xy, tbn);
	vec4 albedo = texture(SAMPLER(albedoTex), texCoord);

	if (alphaTest == 1 && albedo.a < 0.5)
		discard;

#ifdef EDITOR
	color_out = vec4(albedo.rgb * CalcEditorLight(normal, miscMaps.b), 1.0);
#else
	float roughness = mix(roughnessMin, roughnessMax, miscMaps.r);
	DeferredOut(albedo.rgb / albedo.a, normal, roughness, miscMaps.g, miscMaps.b);
#endif
}
