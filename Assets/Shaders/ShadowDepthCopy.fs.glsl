#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(binding=0) uniform sampler2D sourceShadowMap;

void main()
{
	gl_FragDepth = texture(sourceShadowMap, texCoord_in).r;
}
