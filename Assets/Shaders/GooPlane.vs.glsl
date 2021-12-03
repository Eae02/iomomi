#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec4 edgeDists_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec4 edgeDists_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

const vec4 waves[] = vec4[] (vec4(0.3, 0.3, 0.5, 0.05), vec4(-0.5, 0.5, 1.0, 0.05), vec4(0.3, -0.5, 0.8, 0.05));

void main()
{
	vec3 posTime = vec3(position_in.xz, renderSettings.gameTime);
	float yoffset = 0;
	for (uint i = 0; i < waves.length(); i++) {
		yoffset += sin(dot(posTime, waves[i].xyz)) * waves[i].w;
	}
	
	worldPos_out = position_in + vec3(0, yoffset, 0);
	edgeDists_out = edgeDists_in;
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
}
