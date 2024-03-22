#version 450 core

#include "Inc/EditorLight.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec3 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;
layout(location=3) in vec2 gridTexCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform texture2DArray albedoTex;
layout(binding=2) uniform texture2DArray normalMapTex;
layout(binding=3) uniform texture2D gridTex;
layout(binding=4) uniform texture2D noDrawTex;

layout(binding=5) uniform sampler uSampler;

void main()
{
	vec3 surfNormal = normalize(normal_in);
	vec3 surfTangent = normalize(tangent_in);
	surfTangent = normalize(surfTangent - dot(surfNormal, surfTangent) * surfNormal);
	mat3 tbn = mat3(surfTangent, cross(surfTangent, surfNormal), surfNormal);
	
	vec3 color, normal;
	
	vec2 gradX = dFdx(texCoord_in.xy);
	vec2 gradY = dFdy(texCoord_in.xy);
	
	if (texCoord_in.z < -0.5)
	{
		color = textureGrad(sampler2D(noDrawTex, uSampler), texCoord_in.xy, gradX, gradY).rgb;
		normal = surfNormal;
	}
	else
	{
		color = textureGrad(sampler2DArray(albedoTex, uSampler), texCoord_in.xyz, gradX, gradY).rgb;
		normal = normalMapToWorld(textureGrad(sampler2DArray(normalMapTex, uSampler), texCoord_in.xyz, gradX, gradY).xy, tbn);
	}
	
	color *= CalcEditorLight(normal, 1.0);
	
	float gridA = texture(sampler2D(gridTex, uSampler), gridTexCoord_in).r;
	color = mix(color, vec3(1.0), gridA);
	
	color_out = vec4(color, 1.0);
}
