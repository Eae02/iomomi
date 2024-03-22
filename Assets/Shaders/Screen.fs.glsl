#version 450 core

#include "Inc/DeferredGeom.glh"
#include "Inc/NormalMap.glh"

layout(location = 0) in vec3 worldPos_in;
layout(location = 1) in vec2 texCoord_in;
layout(location = 2) in vec3 normal_in;
layout(location = 3) in vec3 tangent_in;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(binding = 1) uniform texture2D colorTexture;
layout(binding = 2) uniform sampler colorSampler;

void main()
{
	float texOffset = pow(max(0, abs(fract(texCoord_in.y - renderSettings.gameTime * 0.3) - 0.5) - 0.4), 4);

	vec2 sampleCoord = vec2(texCoord_in.x + texOffset, EG_FLIPGL(texCoord_in.y));
	vec3 albedo = texture(sampler2D(colorTexture, colorSampler), sampleCoord).rgb;

	DeferredOut(albedo, normal_in, 0.5, 0.0, 1.0);
}
