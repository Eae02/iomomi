#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in mat3x4 worldTransform_in;
layout(location=5) in vec2 textureScale_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec2 texCoord_out;

void main()
{
	mat4 worldTransform = transpose(mat4(worldTransform_in));
	worldPos_out = (worldTransform * vec4(position_in, 1.0)).xyz;
	texCoord_out = textureScale_in * texCoord_in;
}
