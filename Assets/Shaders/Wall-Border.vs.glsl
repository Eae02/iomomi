#version 450 core

layout(location=0) in vec4 position_in;
layout(location=1) in vec3 normal1_in;
layout(location=2) in vec3 normal2_in;

layout(location=0) out float dash_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	dash_out = position_in.w;
	vec3 toEye = renderSettings.cameraPosition - position_in.xyz;
	if (dot(toEye, normal1_in) < 0.0 && dot(toEye, normal2_in) < 0.0)
		gl_Position = renderSettings.viewProjection * vec4(position_in.xyz, 1.0);
	else
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
}
