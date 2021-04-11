#version 450 core

#pragma variants VGame VEditor

layout(location=0) in vec2 texCoord_in;
layout(location=1) in vec3 worldPos_in;
layout(location=2) in vec2 ypc_in;

layout(location=0) out vec4 color_out;

const vec3 COLOR = vec3(0.1, 0.7, 0.9);
const vec3 COLOR_RED = vec3(1.0, 0.2, 0.3) * 2;

const int OFF_SAMPLES = 4;
const int DUP_LINES = 6;
const float PI = 3.141;
const float OFFSET_SCALE = 0.8;
const float OFFSET_SCALE_RED = 1.5;
const float LINE_SPACING = 0.5;
const float LINE_WIDTH = 0.003;
const float MAX_INTENSITY = 0.9;
const float EDGE_FADE_DIST = 0.2;

layout(binding=1) uniform sampler2D noiseTex;

float lineIntensity(float lx, float tx)
{
	float dist = tx - lx * LINE_SPACING;
	if (abs(dist) < 0.01 && length(vec2(dFdx(dist), dFdy(dist))) > abs(dist * 2.0))
		dist = 0;
	float ldist = (max(abs(dist), LINE_WIDTH) - LINE_WIDTH * MAX_INTENSITY);
	return 1.0 / ldist;
}

layout(push_constant) uniform PC
{
	vec3 position;
	float opacity;
	vec4 tangent;
	vec4 bitangent;
	uint blockedAxis;
} pc;

#if defined(VGame)
const int NUM_INTERACTABLES = 8;

layout(binding=2, std140) uniform BarrierSettingsUB
{
	uvec4 iaDownAxis[NUM_INTERACTABLES / 4];
	vec4 iaPosition[NUM_INTERACTABLES];
	float gameTime;
};

layout(binding=3) uniform sampler2D waterDepth;
layout(binding=4) uniform sampler2D blurredGlassDepth;
layout(binding=5) uniform sampler2D waterDistanceTex;

#define WATER_DEPTH_OFFSET 0.15
#include "Water/WaterTransparent.glh"
#endif

#ifdef VEditor
bool CheckWaterDiscard() { return false; }
const float gameTime = 0;
#endif

void main()
{
	if (CheckWaterDiscard())
	{
		color_out = vec4(0);
		return;
	}
	
#ifdef VGame
	vec2 screenCoord = ivec2(gl_FragCoord.xy) / vec2(textureSize(blurredGlassDepth, 0).xy);
	if (gl_FragCoord.z < texture(blurredGlassDepth, screenCoord).r)
		discard;
#endif
	
	vec2 scaledTC = texCoord_in * vec2(pc.tangent.w, pc.bitangent.w);
	float negScale = 0;
	
	float edgeDist = min((ypc_in.y - abs(ypc_in.x)) / EDGE_FADE_DIST, 1.0);
	
#ifdef VGame
	float intensity = 0;
	float stretch = 0;
	for (int i = 0; i < NUM_INTERACTABLES; i++)
	{
		vec3 toObject = worldPos_in - iaPosition[i].xyz;
		float d = length(toObject);
		uint da = iaDownAxis[i / 4][i % 4];
		
		if (da == pc.blockedAxis)
		{
			negScale += 1.0 - smoothstep(1.0, 1.5, d);
		}
		else if (da != 3)
		{
			float ns = exp(-d * d) * dot(toObject, pc.tangent.xyz);
			stretch = abs(ns) > abs(stretch) ? ns : stretch;
		}
	}
	
	float waterDist = texture(waterDistanceTex, texCoord_in).r;
	negScale += 1.0 - smoothstep(0.5, 0.8, waterDist);

	scaledTC.x -= 0.3 * stretch * edgeDist;
	negScale = min(negScale, 1.0);
#endif
	
#ifdef VEditor
	float intensity = 5;
#endif
	
	float centerLn = floor(scaledTC.x / LINE_SPACING);
	float offScale = edgeDist * mix(OFFSET_SCALE, OFFSET_SCALE_RED, negScale) / DUP_LINES;
	
	for (int d = 0; d < DUP_LINES; d++)
	{
		float off = -0.5 * OFF_SAMPLES;
		for (int j = 0; j < OFF_SAMPLES; j++)
		{
			float sx = ((centerLn * DUP_LINES + d) * OFF_SAMPLES + j) * 0.01;
			float sy = (scaledTC.y + 0.2 * gameTime * (j / float(OFF_SAMPLES - 1)));
			off += texture(noiseTex, vec2(sx, sy)).r;
		}
		intensity += lineIntensity(centerLn + 0.5 + off * offScale, scaledTC.x);
	}
	
	intensity += lineIntensity(centerLn - 0.5, scaledTC.x) * DUP_LINES;
	intensity += lineIntensity(centerLn + 1.5, scaledTC.x) * DUP_LINES;
	intensity *= pc.opacity;
	
	vec3 color = mix(COLOR, COLOR_RED, negScale);
	
	color_out = vec4(color * intensity, min(intensity, 1.0));
}
