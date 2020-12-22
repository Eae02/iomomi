#version 450 core

#pragma variants VDepthMin VDepthMax

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 spritePos_in;
layout(location=1) in vec3 eyePos_in;

#define DEFINE_SPHERE_TO_NDC
#include "WaterCommon.glh"

#ifdef VDepthMax
layout(binding=1) uniform sampler2D geometryDepthSampler;
#endif

void main()
{
	float r2 = dot(spritePos_in, spritePos_in);
	if (r2 > 1.0)
		discard;
	
	vec3 ndcFront = sphereToNDC(r2, 1);
#ifdef VDepthMax
	vec3 ndcBack = sphereToNDC(r2, -1);
	vec2 scrPos = ndcFront.xy * vec2(0.5, EG_OPENGL ? 0.5 : -0.5) + 0.5;
	float inputDepth = texture(geometryDepthSampler, scrPos).r;
	if (EG_OPENGL)
		inputDepth = inputDepth * 2 - 1;
	if (linearizeDepth(ndcBack.z) > linearizeDepth(inputDepth) + 0.6)
		discard;
	gl_FragDepth = min(ndcBack.z, inputDepth);
#else
	gl_FragDepth = ndcFront.z;
#endif
}
