#version 450 core

#pragma variants VDefault VEditor VPlanarRefl

#ifdef VDefault
#include "Inc/DeferredGeom.glh"
#else
layout(location=0) out vec4 color_out;
#endif

#include "Inc/RenderSettings.glh"

layout(set=0, binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(binding=1) uniform sampler2D hexSampler;

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;

const float ROUGHNESS = 0.4;

const vec3 COLOR = vec3(0.12, 0.9, 0.7);

void main()
{
	vec2 patternPos = texCoord_in * 2 - 1;
	
	float hex = texture(hexSampler, patternPos * 4).r;
	
	float a = dot(patternPos, patternPos);
	float intensity = (sin(a * 20 - renderSettings.gameTime * 10 + hex * 3.1415 * 0.25) * 0.5 + 0.5) / (a + 1);
	
	vec3 color = COLOR * mix(0.5, 2.0, intensity);
	
#ifdef VPlanarRefl
	color_out = vec4(color, 1.0);
#endif
	
#ifdef VEditor
	color_out = vec4(color, 1.0);
#endif
	
#ifdef VDefault
	DeferredOut(color, normal_in, ROUGHNESS, 1.0, AO_PRELIT);
#endif
}
