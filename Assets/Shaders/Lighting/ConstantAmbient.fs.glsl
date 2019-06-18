#version 450 core

#pragma variants VDefault VMSAA

layout(location=0) out vec4 color_out;

layout(constant_id=100) const int numSamples = 1;

#ifdef VMSAA
layout(binding=0) uniform sampler2DMS gbColor1Sampler;
#else
layout(binding=0) uniform sampler2D gbColor1Sampler;
#endif

layout(push_constant) uniform PC
{
	vec3 ambient;
};

void main()
{
	color_out = vec4(0, 0, 0, 1);
	
	for (int i = 0; i < numSamples; i++)
	{
		vec4 c1 = texelFetch(gbColor1Sampler, ivec2(gl_FragCoord.xy), i);
		color_out.rgb += c1.xyz * (c1.w > 0.99 ? vec3(1.0) : (ambient * c1.w));
	}
	
	color_out.rgb /= float(numSamples);
}
