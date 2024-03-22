#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(set=0, binding=0) uniform texture2D selTexture;
layout(set=0, binding=1) uniform sampler selTextureSampler;

const int DIST = 4;

layout(set=0, binding=2) uniform Params_UseDynamicOffset
{
	vec3 color;
};

void main()
{
	float i0 = textureOffset(sampler2D(selTexture, selTextureSampler), texCoord_in, ivec2(-DIST, 0)).r;
	float i1 = textureOffset(sampler2D(selTexture, selTextureSampler), texCoord_in, ivec2( DIST, 0)).r;
	float i2 = textureOffset(sampler2D(selTexture, selTextureSampler), texCoord_in, ivec2(0, -DIST)).r;
	float i3 = textureOffset(sampler2D(selTexture, selTextureSampler), texCoord_in, ivec2(0,  DIST)).r;
	color_out = vec4(color, (1 - texture(sampler2D(selTexture, selTextureSampler), texCoord_in).r) * max(max(i0, i1), max(i2, i3)));
}
