#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(location=0) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

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
