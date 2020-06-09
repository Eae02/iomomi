#version 450 core

#pragma variants VDefault

layout(location=0) in vec2 position_in;
layout(location=1) in mat4 worldTransform_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec2 texCoord_out;

layout(location=2) out vec3 normal_out;
layout(location=3) out vec3 tangent_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(push_constant) uniform PC
{
	vec4 plane;
	float roughness;
	float globalAlpha;
};

void main()
{
	texCoord_out = position_in * 0.5 + 0.5;
	worldPos_out = (worldTransform_in * vec4(position_in.x, 0.01, position_in.y, 1)).xyz;
	normal_out = worldTransform_in[1].xyz;
	tangent_out = worldTransform_in[0].xyz;
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1);
}
