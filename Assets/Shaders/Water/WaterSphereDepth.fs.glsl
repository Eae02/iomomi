#version 450 core

#include "Simulation/WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 spritePos_in;
layout(location=1) in vec3 eyePos_in;

void main()
{
	float r2 = dot(spritePos_in, spritePos_in);
	if (r2 > 1.0)
		discard;
	vec3 normalES = vec3(spritePos_in, sqrt(1.0 - r2));
	vec4 ppsPos = renderSettings.projectionMatrix * vec4(eyePos_in + normalES * PARTICLE_RADIUS, 1.0);
	gl_FragDepth = ppsPos.z / ppsPos.w;
}
