#version 420 core

layout(location=0) out vec4 color_out;

layout(location=0) in vec2 texCoord_in;
layout(location=1) in float opacity_in;
layout(location=2) flat in int additive_in;

layout(binding=1) uniform sampler2D diffuseSampler;
layout(binding=2) uniform sampler2D depthSampler;

const float FADE_BEGIN_DEPTH = 0.005;

void main()
{
	color_out = texture(diffuseSampler, texCoord_in);
	
	float hDepth = texture(depthSampler, vec2(gl_FragCoord.xy) / vec2(textureSize(depthSampler, 0).xy)).r;
	if (gl_FragCoord.z > hDepth)
		discard;
	
	float opacity = opacity_in * min((hDepth - gl_FragCoord.z) / FADE_BEGIN_DEPTH, 1.0);
	
	if (additive_in != 0)
	{
		color_out.a = 0.0;
		color_out.rgb *= opacity;
	}
	else
	{
		color_out.a *= opacity;
		color_out.rgb *= color_out.a;
	}
}
