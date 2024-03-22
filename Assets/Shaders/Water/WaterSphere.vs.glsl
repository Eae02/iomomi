#version 450 core

#pragma variants VDepthMin VDepthMax

#include "WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location = 0) out vec2 spritePos_out;
layout(location = 1) out vec3 eyePos_out;

#ifdef VDepthMax
const float Z_SHIFT_ES = -W_PARTICLE_RENDER_RADIUS;
#endif

#ifdef VDepthMin
layout(location = 2) out float glowIntensity_out;
const float Z_SHIFT_ES = W_PARTICLE_RENDER_RADIUS;
#endif

layout(set = 0, binding = 1, std430) readonly buffer ParticlesBuffer
{
	uvec4 particleData[];
};

const vec2 QUAD_VERTICES[] = vec2[](vec2(-1, -1), vec2(-1, 1), vec2(1, 1), vec2(1, 1), vec2(1, -1), vec2(-1, -1));

void main()
{
	uint dataIndex = gl_VertexIndex / 6;
	vec2 positionIn = QUAD_VERTICES[gl_VertexIndex % 6];

	spritePos_out = positionIn;

	uvec4 data = particleData[dataIndex];

	vec3 eyePosCenter = (renderSettings.viewMatrix * vec4(uintBitsToFloat(data.xyz), 1.0)).xyz;

	eyePos_out = eyePosCenter + vec3(positionIn * W_PARTICLE_RENDER_RADIUS, 0);

#ifdef VDepthMin
	uint glowTime = dataBitsGetGlowTime(data.w);
	glowIntensity_out = float(glowTime) / float(W_MAX_GLOW_TIME);
#endif

	gl_Position = renderSettings.projectionMatrix * vec4(eyePos_out.xyz, 1.0);

	vec4 posShifted = renderSettings.projectionMatrix * vec4(eyePosCenter.xy, eyePosCenter.z + Z_SHIFT_ES, 1.0);
	gl_Position.z = posShifted.z / posShifted.w * gl_Position.w;
}
