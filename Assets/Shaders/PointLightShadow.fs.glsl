#version 450 core

#pragma variants VDefault VAlbedo

layout(location=0) in vec3 position_in;

#ifdef VAlbedo
layout(location=1) in vec2 texCoord_in;

layout(set=0, binding=1) uniform sampler2D albedo;
#endif

layout(push_constant) uniform PC
{
	mat4 lightMatrix;
	vec4 lightSphere;
} pc;

void main()
{
#ifdef VAlbedo
	if (texture(albedo, texCoord_in).a < 0.5)
		discard;
#endif
	gl_FragDepth = distance(position_in, pc.lightSphere.xyz) / pc.lightSphere.w;
}
