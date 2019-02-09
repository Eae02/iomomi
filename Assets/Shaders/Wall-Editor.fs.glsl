#version 450 core

layout(location=0) in vec3 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray albedoSampler;
layout(binding=2) uniform sampler2DArray normalMapSampler;
layout(binding=3) uniform sampler2D gridSampler;

void main()
{
	vec3 surfNormal = normalize(normal_in);
	vec3 surfTangent = normalize(tangent_in);
	surfTangent = normalize(surfTangent - dot(surfNormal, surfTangent) * surfNormal);
	mat3 tbn = mat3(surfTangent, cross(surfTangent, surfNormal), surfNormal);
	
	vec3 nmNormal = (texture(normalMapSampler, texCoord_in).grb * (255.0 / 128.0)) - vec3(1.0);
	vec3 normal = normalize(tbn * nmNormal);
	
	float light = max(dot(normal, vec3(1, 1, 1)), 0.0) * 0.75 + 0.75;
	
	float gridIntensity = texture(gridSampler, texCoord_in.xy).r;
	vec3 color = texture(albedoSampler, texCoord_in).rgb;
	color = mix(color, vec3(1.0), gridIntensity);
	
	color_out = vec4(color * light, 1.0);
}
