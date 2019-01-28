#version 450 core

#include "../Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec3 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2DArray diffuseSampler;
layout(binding=2) uniform sampler2DArray normalMapSampler;
layout(binding=3) uniform sampler2DArray miscMapSampler;

layout(push_constant) uniform PC
{
	vec4 ambient;
};

const float HEIGHT_SCALE = 0.05;

void main()
{
	vec4 miscMaps = texture(miscMapSampler, texCoord_in);
	color_out = (miscMaps.b * ambient) * texture(diffuseSampler, texCoord_in);
}
