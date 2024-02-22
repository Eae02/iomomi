#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/Utils.glh"
#include "Inc/Light.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D albedoSampler;
layout(binding=2) uniform sampler2D nmSampler;
layout(binding=3) uniform sampler2D mmSampler;

void main()
{
	vec3 sNormal = normalize(normal_in);
	vec3 sTangent = normalize(tangent_in);
	sTangent = normalize(sTangent - dot(sNormal, sTangent) * sNormal);
	mat3 tbn = mat3(sTangent, cross(sTangent, sNormal), sNormal);
	
	vec4 miscMaps = texture(mmSampler, texCoord_in);
	
	GBData gbData;
	gbData.worldPos = worldPos_in;
	gbData.normal = normalMapToWorld(texture(nmSampler, texCoord_in).xy, tbn);
	gbData.albedo = texture(albedoSampler, texCoord_in).rgb;
	gbData.roughness = miscMaps.r;
	gbData.metallic = miscMaps.g;
	gbData.ao = miscMaps.b;
	
	vec3 toEye = normalize(vec3(0.0) - worldPos_in);
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	const vec3 lightRadiance = vec3(1.0, 0.9, 0.9);
	const vec3 ambientLightColor = vec3(0.1);
	const vec3 toLight = vec3(0.0, 1.0, 0.0);
	const float distToLight = 1.0;
	
	vec3 kd = (1.0 - fresnel) * (1.0 - gbData.metallic);
	vec3 color = (gbData.albedo * kd + fresnel * (1 - gbData.roughness)) * ambientLightColor * gbData.ao;
	color += calcDirectReflectance(toLight, distToLight, toEye, fresnel, gbData, lightRadiance, 1.0);
	
	color_out = vec4(color, 1.0);
}
