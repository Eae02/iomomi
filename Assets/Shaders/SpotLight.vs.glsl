#version 450 core

#include "Inc/RenderSettings.glh"

layout(location=0) in vec3 position_in;

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 direction;
	float penumbraBias;
	vec3 directionL;
	float penumbraScale;
	vec3 radiance;
	float width;
} pc;

void main()
{
	vec3 scale = vec3(pc.width, 1.0, pc.width) * pc.range;
	
	vec3 lightDirU = cross(pc.direction, pc.directionL);
	mat3 rotationMatrix = mat3(pc.directionL, pc.direction, lightDirU);
	vec3 worldPos = pc.position + rotationMatrix * (position_in * scale);
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
