#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in mat3x4 worldTransform_in;
layout(location=5) in vec4 textureRange_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec2 texCoord_out;

layout(push_constant) uniform PC
{
	mat4 lightMatrix;
	vec4 lightSphere;
} pc;

void main()
{
	mat4 worldTransform = transpose(mat4(worldTransform_in));
	worldPos_out = (worldTransform * vec4(position_in, 1.0)).xyz;
	texCoord_out = mix(textureRange_in.xy, textureRange_in.zw, texCoord_in);
	gl_Position = pc.lightMatrix * vec4(worldPos_out, 1.0);
}
