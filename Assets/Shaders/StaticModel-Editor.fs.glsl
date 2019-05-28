#version 450 core

#include "Inc/EditorLight.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D albedoSampler;
layout(binding=2) uniform sampler2D nmSampler;
layout(binding=3) uniform sampler2D mmSampler;

layout(push_constant) uniform PC
{
	layout(offset=8) vec2 textureScale;
};

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec2 texCoord = textureScale * texCoord_in;
	vec4 miscMaps = texture(mmSampler, texCoord);
	
	vec3 normal = normalMapToWorld(texture(nmSampler, texCoord).xy, tbn);
	
	vec4 albedo = texture(albedoSampler, texCoord);
	
	if (albedo.a < 0.5)
		discard;
	
	color_out = vec4(albedo.rgb * CalcEditorLight(normal, miscMaps.b), 1.0);
}
