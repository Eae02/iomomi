#version 450 core

#include "Inc/RenderSettings.glh"

layout(location=0) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

layout(set=0, binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

const int NM_SAMPLES = 3;

layout(set=0, binding=1, std140) uniform NMTransformsUB
{
	vec3 color;
	vec4 nmTransforms[NM_SAMPLES * 2];
};

layout(set=0, binding=2) uniform sampler2D normalMap;

const float ROUGHNESS = 0.2;

void main()
{
	
}
