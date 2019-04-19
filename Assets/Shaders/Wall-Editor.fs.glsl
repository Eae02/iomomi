#version 450 core

#include "Inc/EditorLight.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec4 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;
layout(location=3) in vec4 ao_in;

layout(location=0) out vec4 color_out;

layout(binding=2) uniform sampler2DArray albedoSampler;
layout(binding=3) uniform sampler2DArray normalMapSampler;
layout(binding=4) uniform sampler2D gridSampler;
layout(binding=5) uniform sampler2D noDrawSampler;

void main()
{
	vec3 surfNormal = normalize(normal_in);
	vec3 surfTangent = normalize(tangent_in);
	surfTangent = normalize(surfTangent - dot(surfNormal, surfTangent) * surfNormal);
	mat3 tbn = mat3(surfTangent, cross(surfTangent, surfNormal), surfNormal);
	
	vec3 color, normal;
	if (texCoord_in.z < -0.5)
	{
		color = texture(noDrawSampler, texCoord_in.xy).rgb;
		normal = surfNormal;
	}
	else
	{
		color = texture(albedoSampler, texCoord_in.xyz).rgb;
		normal = normalMapToWorld(texture(normalMapSampler, texCoord_in.xyz).xy, tbn);
	}
	
	vec2 ao2 = vec2(min(ao_in.x, ao_in.y), min(ao_in.w, ao_in.z));
	ao2 = pow(clamp(ao2, vec2(0.0), vec2(1.0)), vec2(0.5));
	float ao = ao2.x * ao2.y;
	
	color *= CalcEditorLight(normal, ao);
	
	float gridIntensity = texture(gridSampler, texCoord_in.xy * texCoord_in.w).r;
	color = mix(color, vec3(1.0), gridIntensity);
	
	color_out = vec4(color, 1.0);
}
