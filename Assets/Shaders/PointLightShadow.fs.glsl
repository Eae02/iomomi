#version 450 core

#pragma variants VNoAlphaTest VAlphaTest

layout(location=0) in vec3 position_in;

#ifdef VAlphaTest
layout(location=1) in vec2 texCoord_in;

layout(set=0, binding=1) uniform sampler2D albedo;
#endif

layout(constant_id=10) const float depthBiasFront = 0.05;
layout(constant_id=11) const float depthBiasBack = 0.05;

layout(push_constant) uniform PC
{
	mat4 lightMatrix;
	vec4 lightSphere;
} pc;

void main()
{
#ifdef VAlphaTest
	if (texture(albedo, texCoord_in).a < 0.5)
		discard;
#endif
	float bias = gl_FrontFacing ? depthBiasFront : depthBiasBack;
	gl_FragDepth = clamp((distance(position_in, pc.lightSphere.xyz) + bias) / pc.lightSphere.w, 0, 1);
}
