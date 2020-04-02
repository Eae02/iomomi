#version 450 core

#include "Inc/DeferredGeom.glh"
#include "Inc/NormalMap.glh"

layout(location=0) in vec3 worldPos_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(binding=1) uniform sampler2D colorSampler;

void main()
{
	float texOffset = pow(max(0, abs(fract(texCoord_in.y - renderSettings.gameTime * 0.3) - 0.5) - 0.4), 4);
	
	vec3 albedo = texture(colorSampler, texCoord_in + vec2(texOffset, 0)).rgb;
	
	DeferredOut(albedo, normal_in, 0.5, 0.0, 1.0);
}
