#version 450 core

layout(location=0) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	vec3 switchPos;
	mat3 rotationMatrix;
};

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(binding=1) uniform sampler2D emissionMap;

const vec3 COLOR = vec3(0.3, 0.8, 1.5) * 3;

const float MAX_RAY_DIST = 2.0;
const int RAY_STEPS = 32;

layout(binding=2, std140) uniform LightSettingsUB
{
	float tMax;
	float inverseMaxY;
	vec4 samplePoints[RAY_STEPS / 4];
};

const float MESH_SCALE = 0.6;
const float EMI_MAP_SCALE = 0.5;

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
		vec2 samplePos = vec2(dot(pos, EMI_PAN_DIRECTIONS[i]) + renderSettings.gameTime, dot(pos, oPan));
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
	vec3 toSwitchLocal = transpose(rotationMatrix) * (worldPos - switchPos);
	
	vec2 emiPos = toSwitchLocal.xz / (MESH_SCALE * (tMax * toSwitchLocal.y + 1));
	float emiIntensity = sampleEmiMap(emiPos);
	float atten = lenSq(toSwitchLocal * vec3(1, 3, 1));
	
	return COLOR * max((emiIntensity / atten) * (1.0 - toSwitchLocal.y * inverseMaxY), 0.0);
}

void main()
{
	vec3 toEye = renderSettings.cameraPosition - worldPos_in;
	float distToEye = length(toEye);
	toEye /= distToEye;
	
	float rayDist = min(distToEye, MAX_RAY_DIST);
	float ds = rayDist / float(RAY_STEPS);
	
	vec3 color = vec3(0.0);
	for (int i = 0; i < RAY_STEPS; i++)
	{
		color += volLight(worldPos_in + toEye * (rayDist * samplePoints[i / 4][i % 4])) * ds;
	}
	
	color_out = vec4(color, 0.0);
}