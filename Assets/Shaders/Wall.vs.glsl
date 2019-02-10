#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in uvec4 texCoordAO_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec3 texCoord_out;
layout(location=1) out vec3 normal_out;
layout(location=2) out vec3 tangent_out;
layout(location=3) out vec2 ao_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	normal_out = normal_in;
	tangent_out = tangent_in;
	
	vec3 biTangent = cross(tangent_in, normal_in);
	texCoord_out = vec3(texCoordAO_in.xy / 255.0, texCoordAO_in.z);
	
	const float AO_BIAS = 0.25; //Increasing this makes the maximum amount of AO less intense.
	const float AO_SCALE = 3.0; //Decreasing this makes AO continue further from walls.
	
	vec2 ao = vec2(texCoordAO_in.w & 0xFU, (texCoordAO_in.w >> 4) & 0xFU) / 15.0;
	ao_out = ao * AO_SCALE + AO_BIAS;
	
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
