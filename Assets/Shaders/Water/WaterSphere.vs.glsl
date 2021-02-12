#version 450 core

#pragma variants VDepthMin VDepthMax

#include "WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 position_in;

layout(location=0) out vec2 spritePos_out;
layout(location=1) out vec3 eyePos_out;

#ifdef VDepthMax
const float Z_SHIFT_ES = -PARTICLE_RADIUS;

layout(push_constant) uniform PC
{
	uint lastParticleIndex;	
};
#endif

#ifdef VDepthMin
layout(location=2) out float glowIntensity_out;
const float GLOW_DURATION = 0.5;
const float Z_SHIFT_ES = PARTICLE_RADIUS;
#endif

layout(binding=1, std430) readonly buffer ParticlesBuffer
{
	vec4 particleData[];
};

void main()
{
#ifdef VDepthMin
	uint dataIndex = gl_InstanceIndex;
#else
	uint dataIndex = lastParticleIndex - gl_InstanceIndex;
#endif
	
	spritePos_out = position_in;
	
	vec3 eyePosCenter = (renderSettings.viewMatrix * vec4(particleData[dataIndex].xyz, 1.0)).xyz;
	
	eyePos_out = eyePosCenter + vec3(position_in * PARTICLE_RADIUS, 0);
	
#ifdef VDepthMin
	glowIntensity_out = 1 - clamp(((renderSettings.gameTime + 100) - particleData[dataIndex].w) / GLOW_DURATION, 0, 1);
#endif
	
	gl_Position = renderSettings.projectionMatrix * vec4(eyePos_out.xyz, 1.0);
	
	vec4 posShifted = renderSettings.projectionMatrix * vec4(eyePosCenter.xy, eyePosCenter.z + Z_SHIFT_ES, 1.0);
	gl_Position.z = posShifted.z / posShifted.w * gl_Position.w;
}
