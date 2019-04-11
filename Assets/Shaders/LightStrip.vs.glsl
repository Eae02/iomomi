#version 450 core

#pragma variants VDefault VPlanarRefl

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in mat4 worldTransform_in;
layout(location=6) in float lightOffset_in;

layout(location=0) out vec2 texCoord_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

#ifdef VPlanarRefl
layout(push_constant) uniform PC
{
	layout(offset=32) vec4 plane;
};
#include "Inc/PlanarRefl.glh"
#endif

void main()
{
	vec3 worldPos = (worldTransform_in * vec4(position_in, 1.0)).xyz;
	texCoord_out = vec2(0, lightOffset_in) + texCoord_in;
	
#ifdef VPlanarRefl
	planarReflection(plane, worldPos);
#endif
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
