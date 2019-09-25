#version 450 core

layout(location=0) in flat uint mode_in;
layout(location=1) in vec2 pos2_in;
layout(location=2) in vec2 size_in;

#include "ForceField.glh"

layout(location=0) out vec4 color_out;

const float RAD = 0.2;
const vec3 COLOR = pow(vec3(0.14, 0.56, 0.77), vec3(2.2));

const float MIN_INTENSITY = 0.2;

void main()
{
	if (mode_in == MODE_BOX)
	{
		vec2 fromEdge = size_in - abs(pos2_in);
		float a = pow(1.0 - clamp(min(fromEdge.x, fromEdge.y) / RAD, 0, 1) * (1.0 - MIN_INTENSITY), 2.0);
		color_out = vec4(COLOR * a, a);
	}
}
