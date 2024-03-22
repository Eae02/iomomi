#version 450 core

layout(location=0) in vec3 localPos_in;
layout(location=1) in vec3 down_in;
layout(location=2) in vec3 downTangent_in;
layout(location=3) in vec2 minMaxIntensity_in;

layout(location=0) out vec4 color_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/Utils.glh"

const float ANIM_GAIN_TIME = 0.05;
const float ANIM_FALL_TIME = 0.2;
const float ANIM_ZERO_TIME = 0.1;
const float ANIM_TOT_TIME = ANIM_GAIN_TIME + ANIM_FALL_TIME + ANIM_ZERO_TIME;
const vec3 COLOR = vec3(0.12, 0.9, 0.7);
const float ANIM_SPEED = 0.5;

layout(binding=1) uniform texture2D noiseTexture;
layout(binding=2) uniform sampler noiseSampler;

float animationIntensity(float tc)
{
	float py = fract(tc / ANIM_TOT_TIME) * ANIM_TOT_TIME;
	if (py < ANIM_GAIN_TIME)
		return py / ANIM_GAIN_TIME;
	return max(1 - (py - ANIM_GAIN_TIME) / ANIM_FALL_TIME, 0);
}

void main()
{
	vec3 sphereCoord = normalize(localPos_in);
	float tc = -dot(sphereCoord, down_in);
	
	vec2 noiseSamplePos = vec2(
		atan(dot(localPos_in, downTangent_in), dot(localPos_in, cross(down_in, downTangent_in))),
		dot(localPos_in, down_in)
	);
	tc += texture(sampler2D(noiseTexture, noiseSampler), noiseSamplePos * 0.5).r * 0.1;
	
	float intensity = pow(animationIntensity(tc + renderSettings.gameTime * ANIM_SPEED), 5);
	color_out = vec4(COLOR * mix(minMaxIntensity_in.x, minMaxIntensity_in.y, intensity), 0);
}
