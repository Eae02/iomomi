#version 450 core

layout(location=0) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	vec3 switchPos;
	float intensity;
	mat3 rotationMatrix;
} pc;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(binding=1) uniform sampler2D emissionMap;

const vec3 COLOR = vec3(0.12, 0.9, 0.7) * 3;

const float MAX_RAY_DIST = 2.5;

layout(binding=2, std140) uniform LightSettingsUB
{
	float tMax;
	float inverseMaxY;
	float oneOverRaySteps;
	int quarterRaySteps;
	vec4 samplePoints[10];
};

layout(set=1, binding=0) uniform sampler2D waterDepth;
#include "Water/WaterTransparent.glh"

const float MESH_SCALE = 0.6;
const float EMI_MAP_SCALE = 0.5;
const float ANIMATION_SPEED = 0.75;

const vec2[] EMI_PAN_DIRECTIONS = vec2[] (
	vec2(1, 1),
	vec2(-1, 1),
	vec2(0, -1)
);

float sampleEmiMap(vec2 pos)
{
	float intensity = clamp((1.0 - length(pos)) * 3, 0.0, 1.0);
	if (intensity < 1E-6)
		return 0.0;
	
	float brightness = 0.0;
	for (int i = 0; i < EMI_PAN_DIRECTIONS.length(); i++)
	{
		vec2 oPan = vec2(-EMI_PAN_DIRECTIONS[i].y, EMI_PAN_DIRECTIONS[i].x);
		vec2 samplePos = vec2(dot(pos, EMI_PAN_DIRECTIONS[i]) + ANIMATION_SPEED * renderSettings.gameTime, dot(pos, oPan));
		brightness += texture(emissionMap, samplePos * EMI_MAP_SCALE).r;
	}
	return brightness * intensity / EMI_PAN_DIRECTIONS.length();
}

float lenSq(vec3 v)
{
	return dot(v, v);
}

vec3 volLight(vec3 worldPos)
{
	vec3 toSwitchLocal = transpose(pc.rotationMatrix) * (worldPos - pc.switchPos);
	
	vec2 emiPos = toSwitchLocal.xz / (MESH_SCALE * (tMax * toSwitchLocal.y + 1.0));
	float emiIntensity = sampleEmiMap(emiPos);
	float atten = max(lenSq(toSwitchLocal * vec3(1, 3, 1)), 0.01);
	
	return COLOR * max((emiIntensity / atten) * (1.0 - toSwitchLocal.y * inverseMaxY), 0.0);
}

void main()
{
	if (CheckWaterDiscard())
	{
		color_out = vec4(0);
		return;
	}
	
	vec3 toEye = renderSettings.cameraPosition - worldPos_in;
	float distToEye = length(toEye);
	toEye /= distToEye;
	
	float rayDist = min(distToEye, MAX_RAY_DIST);
	float ds = rayDist * oneOverRaySteps;
	
	vec3 color = vec3(0.0);
	for (int i = 0; i < quarterRaySteps; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			color += volLight(worldPos_in + toEye * (rayDist * samplePoints[i][j])) * ds;
		}
	}
	
	color_out = vec4(color * pc.intensity, 0.0);
}
