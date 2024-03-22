#version 420 core
#extension GL_EXT_samplerless_texture_functions : enable

layout(location=0) out vec4 color_out;

layout(location=0) in vec2 texCoord_in;
layout(location=1) in float opacity_in;
layout(location=2) flat in int additive_in;

layout(set=0, binding=1) uniform texture2D colorTexture;
layout(set=0, binding=2) uniform sampler colorSampler;

layout(set=1, binding=0) uniform texture2D depthTexture_UF;

const float FADE_BEGIN_DEPTH = 0.005;

void main()
{
	color_out = texture(sampler2D(colorTexture, colorSampler), texCoord_in);
	
	float hDepth = texelFetch(depthTexture_UF, ivec2(gl_FragCoord.xy), 0).r;
	// if (gl_FragCoord.z > hDepth)
	// 	discard;
	
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
