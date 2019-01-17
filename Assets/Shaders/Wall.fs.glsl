#version 450 core

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec3 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray diffuseSampler;

void main()
{
	color_out = texture(diffuseSampler, texCoord_in);
}
