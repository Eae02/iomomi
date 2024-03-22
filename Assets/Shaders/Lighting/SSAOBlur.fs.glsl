#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out float ssao_out;

const float kernel[] = float[] (0.382928, 0.241732, 0.060598, 0.005977, 0.000229);

layout(set=0, binding=0) uniform Params
{
	vec2 blurVector;
	vec2 texCoordOffset;
};

layout(set=0, binding=1) uniform texture2D inputImage;
layout(set=0, binding=2) uniform sampler linearSampler;

void main()
{
	vec2 centerTC = texCoord_in + texCoordOffset;
	ssao_out = texture(sampler2D(inputImage, linearSampler), centerTC).r * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		ssao_out += texture(sampler2D(inputImage, linearSampler), centerTC + blurVector * i).r * kernel[i];
		ssao_out += texture(sampler2D(inputImage, linearSampler), centerTC - blurVector * i).r * kernel[i];
	}
}
