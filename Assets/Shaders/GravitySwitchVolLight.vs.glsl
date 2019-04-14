#version 450 core

layout(location=0) in vec3 position_in;

layout(location=0) out vec3 worldPos_out;

layout(push_constant) uniform PC
{
	vec3 switchPos;
	mat3 rotationMatrix;
};

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	worldPos_out = rotationMatrix * position_in + switchPos;
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
}
