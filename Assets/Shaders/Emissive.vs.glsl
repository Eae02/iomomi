#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in mat4 worldTransform_in;
layout(location=5) in vec4 color_in;

#include "Inc/RenderSettings.glh"

layout(location=0) out vec4 color_out;

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	vec3 worldPos = (worldTransform_in * vec4(position_in, 1.0)).xyz;
	color_out = color_in;
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}