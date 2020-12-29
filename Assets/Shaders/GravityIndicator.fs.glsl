#version 450 core

layout(location=0) in vec3 localPos_in;
layout(location=1) in vec3 down_in;
layout(location=2) in vec3 downTangent_in;
layout(location=3) in float glow_in;

layout(location=0) out vec4 color_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/Utils.glh"

const float ANIM_GAIN_TIME = 0.1;
const float ANIM_FALL_TIME = 0.5;
const float ANIM_ZERO_TIME = 0.3;
const float ANIM_TOT_TIME = ANIM_GAIN_TIME + ANIM_FALL_TIME + ANIM_ZERO_TIME;
const vec3 COLOR = vec3(0.12, 0.9, 0.7);
const float ANIM_MAX_INTENSITY = 2;
const float ANIM_SPEED = 0.3;

float animationIntensity(vec2 tc)
{
	float py = fract((tc.y) / ANIM_TOT_TIME) * ANIM_TOT_TIME;
	if (py < ANIM_GAIN_TIME)
		return py / ANIM_GAIN_TIME;
	return max(1 - (py - ANIM_GAIN_TIME) / ANIM_FALL_TIME, 0);
}

void main()
{
	float downDist = dot(localPos_in, down_in);
	float texX = atan(dot(localPos_in, downTangent_in), dot(localPos_in, cross(downTangent_in, down_in))) / PI + 0.5;
	float texY = downDist + length(localPos_in - down_in * downDist) - renderSettings.gameTime * ANIM_SPEED;
	
	float intensity = smoothstep(0, 1, animationIntensity(vec2(texX, texY))) * ANIM_MAX_INTENSITY;
	color_out = vec4(COLOR * intensity, 0);
}
