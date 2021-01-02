#version 450 core

#pragma variants VDefault VWithGlowIntensity

#include "WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 position_in;
layout(location=1) in vec4 transform_in;

layout(location=0) out vec2 spritePos_out;
layout(location=1) out vec3 eyePos_out;

#ifdef VWithGlowIntensity
layout(location=2) out float glowIntensity_out;
const float GLOW_DURATION = 0.5;
#endif

void main()
{
	spritePos_out = position_in;
	eyePos_out = (renderSettings.viewMatrix * vec4(transform_in.xyz, 1.0)).xyz + vec3(position_in * PARTICLE_RADIUS, 0);
#ifdef VWithGlowIntensity
	glowIntensity_out = 1 - clamp(((renderSettings.gameTime + 100) - transform_in.w) / GLOW_DURATION, 0, 1);
#endif
	gl_Position = renderSettings.projectionMatrix * vec4(eyePos_out, 1.0);
}
