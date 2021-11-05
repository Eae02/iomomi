#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec3 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec3 texCoord_out;
layout(location=1) out vec3 normal_out;
layout(location=2) out vec3 tangent_out;
layout(location=3) out vec2 gridTexCoord_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	normal_out = normalize(normal_in);
	tangent_out = normalize(tangent_in);
	texCoord_out = texCoord_in;
	gridTexCoord_out = vec2(dot(cross(tangent_out, normal_out), position_in), -dot(tangent_out, position_in));
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
