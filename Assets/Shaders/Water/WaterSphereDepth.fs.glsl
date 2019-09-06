#version 450 core

#pragma variants VDepthMin VDepthMax

#include "WaterCommon.glh"
#include "../Inc/Depth.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 spritePos_in;
layout(location=1) in vec3 eyePos_in;

#ifdef VDepthMax
layout(binding=1) uniform sampler2D geometryDepthSampler;
#endif

void main()
{
	float r2 = dot(spritePos_in, spritePos_in);
	if (r2 > 1.0)
		discard;
	
	vec3 normalES = vec3(spritePos_in, sqrt(1.0 - r2));
#ifdef VDepthMax
	normalES.z = -normalES.z;
#endif
	vec4 ppsPos = renderSettings.projectionMatrix * vec4(eyePos_in + normalES * PARTICLE_RADIUS, 1.0);
	vec3 ndcPos = ppsPos.xyz / ppsPos.w;
	
#ifdef VDepthMax
	vec2 scrPos = ndcPos.xy * vec2(0.5, EG_OPENGL ? 0.5 : -0.5) + 0.5;
	float inputDepth = texture(geometryDepthSampler, scrPos).r;
	if (EG_OPENGL)
		inputDepth = inputDepth * 2 - 1;
	ndcPos.z = min(ndcPos.z, inputDepth);
#endif
	
	gl_FragDepth = ndcPos.z;
}
