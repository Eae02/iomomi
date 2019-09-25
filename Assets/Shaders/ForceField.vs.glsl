#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(location=0) in vec2 pos_in;
layout(location=1) in uint mode_in;
layout(location=2) in vec3 transformOff_in;
layout(location=3) in vec4 transformX_in;
layout(location=4) in vec4 transformY_in;

layout(location=0) out flat uint mode_out;

layout(location=1) out vec2 pos2_out;
layout(location=2) out vec2 size_out;

#include "ForceField.glh"

void main()
{
	mode_out = mode_in;
	vec3 relPos = pos_in.x * transformX_in.xyz + pos_in.y * transformY_in.xyz;
	
	size_out = vec2(transformX_in.w, transformY_in.w);
	pos2_out = pos_in * size_out;
	
	gl_Position = renderSettings.viewProjection * vec4(relPos + transformOff_in, 1.0);
}
