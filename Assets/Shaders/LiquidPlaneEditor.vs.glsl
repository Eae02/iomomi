#version 450 core

layout(location=0) in vec3 position_in;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
