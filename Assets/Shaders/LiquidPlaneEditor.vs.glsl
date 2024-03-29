#version 450 core

layout(location=0) in vec3 position_in;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
