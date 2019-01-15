#version 450 core

layout(location=0) out vec4 color_out;

layout(location=0) in vec4 vColor;
layout(location=1) in vec2 vTexCoord;

layout(binding=0) uniform sampler2D uTexture;

void main()
{
	color_out = vColor * texture(uTexture, vTexCoord);
}
