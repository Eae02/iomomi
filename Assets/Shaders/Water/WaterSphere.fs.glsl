#version 450 core

#pragma variants VDepthMin VDepthMax

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 spritePos_in;
layout(location=1) in vec3 eyePos_in;

#include "WaterCommon.glh"

vec3 sphereToNDC(float r2, float side)
{
	vec3 normalES = vec3(spritePos_in, sqrt(1.0 - r2) * side);
	vec4 ppsPos = renderSettings.projectionMatrix * vec4(eyePos_in + normalES * PARTICLE_RADIUS, 1.0);
	return ppsPos.xyz / ppsPos.w;
}

#ifdef VDepthMax
layout(binding=2) uniform sampler2D geometryDepthSampler;

layout (depth_less) out float gl_FragDepth;
#endif

#ifdef VDepthMin
layout(location=2) in float glowIntensity_in;
layout(location=0) out float glowIntensity_out;

layout (depth_greater) out float gl_FragDepth;
#endif

void main()
{
	float r2 = dot(spritePos_in, spritePos_in);
	if (r2 > 1.0)
		discard;
	
	vec3 ndcFront = sphereToNDC(r2, 1);
#ifdef VDepthMax
	vec3 ndcBack = sphereToNDC(r2, -1);
	float d = depthTo01(ndcBack.z);
	vec2 scrPos = ndcFront.xy * vec2(0.5, EG_OPENGL ? 0.5 : -0.5) + 0.5;
	float inputDepth = texture(geometryDepthSampler, scrPos).r;
	gl_FragDepth = hyperDepth(min(linearizeDepth(d), linearizeDepth(inputDepth) + PARTICLE_RADIUS * 2));
#else
	gl_FragDepth = depthTo01(ndcFront.z);
	glowIntensity_out = glowIntensity_in;
#endif
}
