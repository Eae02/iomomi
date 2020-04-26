#version 450 core

#pragma variants VDefault VPlanarRefl

layout(location=0) in vec3 position_in;
layout(location=1) in mat4 worldTransform_in;
layout(location=5) in vec4 color_in;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(location=0) out vec4 color_out;

#ifdef VPlanarRefl
layout(push_constant) uniform PC
{
	vec4 plane;
};
#include "Inc/PlanarRefl.glh"
#endif

void main()
{
	vec3 worldPos = (worldTransform_in * vec4(position_in, 1.0)).xyz;
	color_out = color_in;
	
#ifdef VPlanarRefl
	planarReflection(plane, worldPos);
#endif
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
