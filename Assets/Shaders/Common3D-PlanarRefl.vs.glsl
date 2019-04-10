#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in mat4 worldTransform_in;

layout(location=0) out vec2 texCoord_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(push_constant) uniform PC
{
	vec4 plane;
};

void main()
{
	vec3 worldPos = (worldTransform_in * vec4(position_in, 1.0)).xyz;
	texCoord_out = texCoord_in;
	
	float distToPlane = dot(plane, vec4(worldPos, 1.0));
	vec3 flippedPos = worldPos - (2 * distToPlane) * plane.xyz;
	
	gl_ClipDistance[0] = distToPlane;
	
	gl_Position = renderSettings.viewProjection * vec4(flippedPos, 1.0);
}
