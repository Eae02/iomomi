#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec3 normal1_in;
layout(location=2) in vec3 normal2_in;

#include "../Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	vec3 toEye = renderSettings.cameraPosition - position_in;
	if (dot(toEye, normal1_in) < 0.0 && dot(toEye, normal2_in) < 0.0)
		gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
	else
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
}
