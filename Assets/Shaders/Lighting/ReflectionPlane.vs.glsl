#version 450 core

layout(location=0) in vec2 position_in;

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(push_constant) uniform PC
{
	vec3 normal;
	float offset;
} pc;

void main()
{
	vec3 tangent;
	if (abs(pc.normal.y) > 0.99)
		tangent = vec3(1, 0, 0);
	else
		tangent = cross(pc.normal, vec3(0, 1, 0));
	
	mat3 rotation = mat3(tangent, cross(tangent, pc.normal), pc.normal);
	gl_Position = renderSettings.viewProjection * vec4(rotation * vec3(position_in, pc.offset), 1);
}
