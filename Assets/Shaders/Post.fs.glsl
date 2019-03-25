#version 450 core

const float exposure = 1.25f;

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D inputSampler;
layout(binding=1) uniform sampler2D bloomSampler;

void main()
{
	vec3 color = texture(inputSampler, texCoord_in).rgb + texture(bloomSampler, texCoord_in).rgb * 0.5;
	
	color_out = vec4(vec3(1.0) - exp(-exposure * color), 1.0);
}
