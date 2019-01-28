#version 450 core

#include "../Inc/Light.glh"
#include "../Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec3 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray albedoSampler;
layout(binding=2) uniform sampler2DArray normalMapSampler;
layout(binding=3) uniform sampler2DArray miscMapSampler;

layout(push_constant) uniform PC
{
	vec3 lightPos;
	vec3 lightRadiance;
};

const float HEIGHT_SCALE = 0.05;

void main()
{
	vec3 surfNormal = normalize(normal_in);
	vec3 surfTangent = normalize(tangent_in);
	surfTangent = normalize(surfTangent - dot(surfNormal, surfTangent) * surfNormal);
	mat3 tbn = mat3(surfTangent, cross(surfTangent, surfNormal), surfNormal);
	
	vec4 miscMaps = texture(miscMapSampler, texCoord_in);
	
	MaterialData materialData;
	materialData.albedo = texture(albedoSampler, texCoord_in).rgb;
	materialData.roughness = mix(0.5, 1.0, miscMaps.r);
	materialData.metallic = miscMaps.g;
	materialData.specularIntensity = 1;
	
	vec3 nmNormal = (texture(normalMapSampler, texCoord_in).grb * (255.0 / 128.0)) - vec3(1.0);
	materialData.normal = normalize(tbn * nmNormal);
	
	vec3 toEye = normalize(renderSettings.cameraPosition - worldPos_in);
	vec3 F = calcFresnel(materialData, toEye);
	color_out = vec4(calcDirectReflectance(lightPos - worldPos_in, toEye, F, materialData, lightRadiance), 1.0);
}
