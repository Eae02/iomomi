#version 450 core

#pragma variants VDepthMin VDepthMax

#include "WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location = 0) in vec2 position_in;

layout(location = 0) out vec2 spritePos_out;
layout(location = 1) out vec3 eyePos_out;

#ifdef VDepthMax
const float Z_SHIFT_ES = -W_PARTICLE_RENDER_RADIUS;

layout(push_constant) uniform PC
{
	uint lastParticleIndex;
};
#endif

#ifdef VDepthMin
layout(location = 2) out float glowIntensity_out;
const float Z_SHIFT_ES = W_PARTICLE_RENDER_RADIUS;
#endif

layout(binding = 1, std430) readonly buffer ParticlesBuffer
{
	uvec4 particleData[];
};

void main()
{
#ifdef VDepthMin
	uint dataIndex = gl_InstanceIndex;
#else
	uint dataIndex = lastParticleIndex - gl_InstanceIndex;
#endif

	spritePos_out = position_in;

	uvec4 data = particleData[dataIndex];

	vec3 eyePosCenter = (renderSettings.viewMatrix * vec4(uintBitsToFloat(data.xyz), 1.0)).xyz;

	eyePos_out = eyePosCenter + vec3(position_in * W_PARTICLE_RENDER_RADIUS, 0);

#ifdef VDepthMin
	uint glowTime = dataBitsGetGlowTime(data.w);
	glowIntensity_out = float(glowTime) / float(W_MAX_GLOW_TIME);
#endif

	gl_Position = renderSettings.projectionMatrix * vec4(eyePos_out.xyz, 1.0);

	vec4 posShifted = renderSettings.projectionMatrix * vec4(eyePosCenter.xy, eyePosCenter.z + Z_SHIFT_ES, 1.0);
	gl_Position.z = posShifted.z / posShifted.w * gl_Position.w;
}
