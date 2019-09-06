#version 450 core

#include "WaterCommon.glh"

layout(location=0) in vec2 spritePos_in;
layout(location=1) in vec3 eyePos_in;

layout(location=0) out float color_out;

void main()
{
	float r2 = dot(spritePos_in, spritePos_in);
	color_out = sqrt(1.0 - min(r2, 1.0)) * PARTICLE_RADIUS * 2 * TRAVEL_DIST_SCALE;
}
