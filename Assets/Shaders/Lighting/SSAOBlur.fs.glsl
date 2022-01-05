#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out float ssao_out;

layout(binding=0) uniform sampler2D inputImage;

const float kernel[] = float[] (0.382928, 0.241732, 0.060598, 0.005977, 0.000229);

layout(push_constant) uniform PC
{
	vec2 blurVector;
	vec2 texCoordOffset;
};

void main()
{
	vec2 centerTC = texCoord_in + texCoordOffset;
	ssao_out = texture(inputImage, centerTC).r * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		ssao_out += texture(inputImage, centerTC + blurVector * i).r * kernel[i];
		ssao_out += texture(inputImage, centerTC - blurVector * i).r * kernel[i];
	}
}
