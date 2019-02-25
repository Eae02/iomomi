#version 450 core

layout(location=0) in vec3 texCoord_in;
layout(location=1) in vec3 normal_in;
layout(location=2) in vec3 tangent_in;
layout(location=3) in vec4 ao_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray albedoSampler;
layout(binding=2) uniform sampler2DArray normalMapSampler;
layout(binding=3) uniform sampler2D gridSampler;
layout(binding=4) uniform sampler2D noDrawSampler;

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
		color = texture(albedoSampler, texCoord_in).rgb;
		vec3 nmNormal = (texture(normalMapSampler, texCoord_in).grb * (255.0 / 128.0)) - vec3(1.0);
		normal = normalize(tbn * nmNormal);
	}
	
	vec2 ao2 = vec2(min(ao_in.x, ao_in.y), min(ao_in.w, ao_in.z));
	ao2 = pow(clamp(ao2, vec2(0.0), vec2(1.0)), vec2(0.5));
	float ao = ao2.x * ao2.y;
	
	const float LIGHT_INTENSITY = 0.5;
	const float BRIGHTNESS = 1.2;
	float light = (max(dot(normal, vec3(1, 1, 1)), 0.0) * LIGHT_INTENSITY + 1.0 - LIGHT_INTENSITY) * ao * BRIGHTNESS;
	
	color *= light;
	
	float gridIntensity = texture(gridSampler, texCoord_in.xy).r;
	color = mix(color, vec3(1.0), gridIntensity);
	
	color_out = vec4(color, 1.0);
}
