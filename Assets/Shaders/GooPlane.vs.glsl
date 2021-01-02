#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec4 edgeDists_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec4 edgeDists_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	worldPos_out = position_in;
	edgeDists_out = edgeDists_in;
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
