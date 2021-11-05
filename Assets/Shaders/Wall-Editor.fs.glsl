#version 450 core

#include "Inc/EditorLight.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec3 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;
layout(location=3) in vec2 gridTexCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray albedoSampler;
layout(binding=2) uniform sampler2DArray normalMapSampler;
layout(binding=3) uniform sampler2D gridSampler;
layout(binding=4) uniform sampler2D noDrawSampler;

layout(push_constant) uniform PC
{
	float gridIntensity;
};

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
	
	color *= CalcEditorLight(normal, 1.0);
	
	float gridA = gridIntensity * texture(gridSampler, gridTexCoord_in).r;
	color = mix(color, vec3(1.0), gridA);
	
	color_out = vec4(color, 1.0);
}
