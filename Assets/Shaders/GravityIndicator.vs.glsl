#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in mat3x4 worldTransform_in;
layout(location=4) in vec4 downAndGlow_in;

layout(location=0) out vec3 localPos_out;
layout(location=1) out vec3 down_out;
layout(location=2) out vec3 downTangent_out;
layout(location=3) out float glow_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	down_out = downAndGlow_in.xyz;
	if (abs(down_out.y) > 0.999)
		downTangent_out = vec3(1, 0, 0);
	else
		downTangent_out = cross(down_out, vec3(0, 1, 0));
	
	glow_out = downAndGlow_in.w;
	
	mat4 worldTransform = transpose(mat4(worldTransform_in));
	vec3 worldPos = (worldTransform * vec4(position_in, 1.0)).xyz;
	localPos_out = worldPos - worldTransform[3].xyz;
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
