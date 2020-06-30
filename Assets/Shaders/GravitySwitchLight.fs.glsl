#version 450 core

#pragma variants VDefault VEditor

#ifdef VDefault
#include "Inc/DeferredGeom.glh"
#else
layout(location=0) out vec4 color_out;
#endif

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(binding=1) uniform sampler2D hexSampler;

layout(location=1) in vec2 texCoord_in;
#ifdef VDefault
layout(location=2) in vec3 normal_in;
#endif

const float ROUGHNESS = 0.4;

const vec3 COLOR = vec3(0.12, 0.9, 0.7);

layout(push_constant) uniform PC
{
	float intensityScale;
	float timeOffset;
};

const float PI = 3.1415;

void main()
{
	vec2 patternPos = texCoord_in * 2 - 1;
	
	float hex = texture(hexSampler, patternPos * 4).r;
	
	float a = dot(patternPos, patternPos);
	float theta = a * -20 + timeOffset + hex * PI * 0.25;
	float intensity = intensityScale * (cos(theta) * 0.5 + 0.5) / (a + 1);
	
	vec3 color = COLOR * mix(0.5, 2.0, intensity);
	
#ifdef VEditor
	color_out = vec4(color, 1.0);
#endif
	
#ifdef VDefault
	DeferredOut(color, normal_in, ROUGHNESS, 1.0, 1.0);
#endif
}
