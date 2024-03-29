#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in mat4 worldTransform_in;
layout(location=6) in float lightOffset_in;

layout(location=0) out vec2 texCoord_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	vec3 worldPos = (worldTransform_in * vec4(position_in, 1.0)).xyz;
	texCoord_out = vec2(0, lightOffset_in) + texCoord_in;
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
