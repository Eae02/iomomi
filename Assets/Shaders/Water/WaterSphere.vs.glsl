#version 450 core

#include "WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 position_in;
layout(location=1) in vec3 transform_in;

layout(location=0) out vec2 spritePos_out;
layout(location=1) out vec3 eyePos_out;

void main()
{
	spritePos_out = position_in;
	eyePos_out = (renderSettings.viewMatrix * vec4(transform_in, 1.0)).xyz + vec3(position_in * PARTICLE_RADIUS, 0);
	gl_Position = renderSettings.projectionMatrix * vec4(eyePos_out, 1.0);
}
