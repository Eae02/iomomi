#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D inputImage;

const float kernel[] = float[] (0.382928, 0.241732, 0.060598, 0.005977, 0.000229);

layout(push_constant) uniform PC
{
	vec2 blurVector;
};

void main()
{
	color_out = texture(inputImage, texCoord_in) * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		color_out += texture(inputImage, texCoord_in + blurVector * i) * kernel[i];
		color_out += texture(inputImage, texCoord_in - blurVector * i) * kernel[i];
	}
}