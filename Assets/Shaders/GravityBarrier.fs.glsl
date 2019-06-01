#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

const vec3 COLOR = vec3(0.1, 0.7, 0.9);

const int OFF_SAMPLES = 4;
const int DUP_LINES = 6;
const float PI = 3.141;
const float OFFSET_SCALE = 0.8;
const float LINE_SPACING = 0.5;
const float INTENSITY_SCALE = 0.0004;
const float LINE_WIDTH = 0.002;
const float MAX_INTENSITY = 0.9;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(set=0, binding=1) uniform sampler2D noiseTex;

float lineIntensity(float lx)
{
	float dist = texCoord_in.x - lx * LINE_SPACING;
	if (abs(dist) < 0.01 && length(vec2(dFdx(dist), dFdy(dist))) > abs(dist * 2.0))
		dist = 0;
	float ldist = (max(abs(dist), LINE_WIDTH) - LINE_WIDTH * MAX_INTENSITY);
	return 1.0 / ldist;
}

layout(push_constant) uniform PC
{
	float baseIntensity;
};

void main()
{
	float intensity = baseIntensity;
	
	float centerLn = floor(texCoord_in.x / LINE_SPACING);
	
	for (int d = 0; d < DUP_LINES; d++) {
		float off = -0.5 * OFF_SAMPLES;
		for (int j = 0; j < OFF_SAMPLES; j++) {
			float sx = ((centerLn * DUP_LINES + d) * OFF_SAMPLES + j) * 0.01;
			float sy = (texCoord_in.y + 0.2 * renderSettings.gameTime * (j / float(OFF_SAMPLES - 1)));
			off += texture(noiseTex, vec2(sx, sy)).r;
		}
		intensity += lineIntensity(centerLn + 0.5 + off * (OFFSET_SCALE / DUP_LINES));
	}
	
	intensity += lineIntensity(centerLn - 0.5) * DUP_LINES;
	intensity += lineIntensity(centerLn + 1.5) * DUP_LINES;
	intensity *= INTENSITY_SCALE;
	
	color_out = vec4(COLOR * intensity, min(intensity, 1.0));
}
