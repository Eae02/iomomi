#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out float ssao_out;

layout(binding=0) uniform sampler2D inputImage;

const float kernel[] = float[] (0.134032, 0.126854, 0.107545, 0.08167, 0.055555, 0.033851, 0.018476, 0.009033);

layout(push_constant) uniform PC
{
	vec2 blurVector;
};

void main()
{
	ssao_out = texture(inputImage, texCoord_in).r * kernel[0];

	for (int i = 1; i < kernel.length(); i++)
	{
		ssao_out += texture(inputImage, texCoord_in + blurVector * i).r * kernel[i];
		ssao_out += texture(inputImage, texCoord_in - blurVector * i).r * kernel[i];
	}
}
