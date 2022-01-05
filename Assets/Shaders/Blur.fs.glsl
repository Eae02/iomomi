#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D inputImage;

const float kernel[] = float[] (0.383103, 0.241843, 0.060626, 0.00598);

layout(push_constant) uniform PC
{
	vec2 blurVector;
	vec2 sampleOffset;
	float mipLevel;
};

void main()
{
	vec2 centerTC = texCoord_in + sampleOffset;
	color_out = textureLod(inputImage, centerTC, mipLevel) * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		color_out += textureLod(inputImage, centerTC + blurVector * i, mipLevel) * kernel[i];
		color_out += textureLod(inputImage, centerTC - blurVector * i, mipLevel) * kernel[i];
	}
	color_out.a = 1;
}
