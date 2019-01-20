#version 450 core

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec3 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray diffuseSampler;

void main()
{
	vec3 shade = abs(normal_in.y) > 0.1 ? vec3(1.0, 0.8, 0.8) : vec3(0.8, 0.8, 1.0);
	
	color_out = vec4(shade, 1.0) * texture(diffuseSampler, texCoord_in);
}
