#version 450 core

#pragma variants VBloom VNoBloom

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D inputSampler;

#ifdef VBloom
layout(binding=1) uniform sampler2D bloomSampler;
#endif

layout (push_constant) uniform PC
{
	float exposure;
#ifdef VBloom
	float bloomIntensity;
#endif
};

void main()
{
	vec3 color = texture(inputSampler, texCoord_in).rgb;
	
#ifdef VBloom
	color += texture(bloomSampler, texCoord_in).rgb * bloomIntensity;
#endif
	
	color_out = vec4(vec3(1.0) - exp(-exposure * color), 1.0);
	
	float vignette = pow(length(texCoord_in - 0.5) / length(vec2(0.55)), 2.0);
	color_out *= 1.0 - vignette;
}
