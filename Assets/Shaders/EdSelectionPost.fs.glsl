#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D selTexture;

const int DIST = 4;

layout(push_constant) uniform PC
{
	vec3 color;
};

void main()
{
	float i0 = textureOffset(selTexture, texCoord_in, ivec2(-DIST, 0)).r;
	float i1 = textureOffset(selTexture, texCoord_in, ivec2( DIST, 0)).r;
	float i2 = textureOffset(selTexture, texCoord_in, ivec2(0, -DIST)).r;
	float i3 = textureOffset(selTexture, texCoord_in, ivec2(0,  DIST)).r;
	color_out = vec4(color, (1 - texture(selTexture, texCoord_in).r) * max(max(i0, i1), max(i2, i3)));
}
