#version 450 core

layout(location=0) in vec4 texCoord_in;
layout(location=3) in vec4 ao_in;

layout(location=0) out vec4 color_out;

layout(binding=2) uniform sampler2DArray albedoSampler;

void main()
{
	vec3 color = texture(albedoSampler, texCoord_in.xyz).rgb;
	
	vec2 ao2 = vec2(min(ao_in.x, ao_in.y), min(ao_in.w, ao_in.z));
	ao2 = pow(clamp(ao2, vec2(0.0), vec2(1.0)), vec2(0.5));
	float ao = ao2.x * ao2.y;
	
	color *= 0.8 + 0.2 * ao;
	
	color_out = vec4(color, 1.0);
}
