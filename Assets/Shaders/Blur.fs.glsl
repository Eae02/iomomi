#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(set=1, binding=0) uniform texture2D uTexture;
layout(set=0, binding=0) uniform sampler uSampler;

const float kernel[] = float[] (0.383103, 0.241843, 0.060626, 0.00598);

layout(set=0, binding=1) uniform Params_UseDynamicOffset
{
	vec2 blurVector;
	vec2 sampleOffset;
	float mipLevel;
};

void main()
{
	vec2 centerTC = texCoord_in + sampleOffset;
	color_out = textureLod(sampler2D(uTexture, uSampler), centerTC, mipLevel) * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		color_out += textureLod(sampler2D(uTexture, uSampler), centerTC + blurVector * i, mipLevel) * kernel[i];
		color_out += textureLod(sampler2D(uTexture, uSampler), centerTC - blurVector * i, mipLevel) * kernel[i];
	}
	color_out.a = 1;
}
