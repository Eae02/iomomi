#version 450 core

#pragma variants VNoAlphaTest VAlphaTest

layout(location=0) in vec3 position_in;
layout(location=2) in mat3x4 worldTransform_in;

#ifdef VAlphaTest
layout(location=1) in vec2 texCoord_in;
layout(location=5) in vec4 textureRange_in;

layout(location=1) out vec2 texCoord_out;
#endif

layout(location=0) out vec3 worldPos_out;

layout(set=0, binding=0) uniform Parameters
{
	mat4 lightMatrix;
	vec4 lightSphere;
};

void main()
{
#ifdef VAlphaTest
	texCoord_out = mix(textureRange_in.xy, textureRange_in.zw, texCoord_in);
#endif
	
	mat4 worldTransform = transpose(mat4(worldTransform_in));
	worldPos_out = (worldTransform * vec4(position_in, 1.0)).xyz;
	gl_Position = lightMatrix * vec4(worldPos_out, 1.0);
}
