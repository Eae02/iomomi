#version 450 core

#include "Inc/EditorLight.glh"

layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D albedoSampler;
layout(binding=2) uniform sampler2D mmSampler;

layout(push_constant) uniform PC
{
	vec4 plane;
	vec2 textureScale;
};

void main()
{
	vec2 texCoord = textureScale * texCoord_in;
	float ao = texture(mmSampler, texCoord).b;
	
	vec3 albedo = texture(albedoSampler, texCoord).rgb;
	
	const float AMBIENT_INTENSITY = 0.1;
	color_out = vec4(albedo * ((1.0 - AMBIENT_INTENSITY) + AMBIENT_INTENSITY * ao), 1.0);
}
