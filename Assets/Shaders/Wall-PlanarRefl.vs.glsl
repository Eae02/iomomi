#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in uvec4 misc_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 texCoord_out;
layout(location=1) out vec4 ao_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(binding=1, std140) uniform MaterialSettingsUB
{
	vec4 matSettings[3]; 
};

layout(push_constant) uniform PC
{
	vec4 plane;
};

void main()
{
	vec3 biTangent = cross(tangent_in, normal_in);
	vec2 texCoord2 = vec2(dot(biTangent, position_in), -dot(tangent_in, position_in));
	texCoord_out = vec4(texCoord2, float(misc_in.x) - 1, 1.0);
	
	if (misc_in.x != 0)
	{
		vec4 mat = matSettings[misc_in.x - 1];
		texCoord_out.xy /= mat.x;
		texCoord_out.w = mat.x;
	}
	
	const float AO_BIAS = 0.05; //Increasing this makes the maximum amount of AO less intense.
	const float AO_SCALE = 3.0; //Decreasing this makes AO continue further from walls.
	
	vec4 ao;
	int i = 0;
	while (i < 4)
	{
		ao[i] = 1 - (misc_in.z >> i) & 1U;
		i++;
	}
	ao_out = ao * AO_SCALE + AO_BIAS;
	
	float distToPlane = dot(plane, vec4(position_in, 1.0));
	vec3 flippedPos = position_in - (2 * distToPlane) * plane.xyz;
	
	gl_ClipDistance[0] = misc_in.y / 127.0 - 1.0;
	gl_ClipDistance[1] = distToPlane;
	
	gl_Position = renderSettings.viewProjection * vec4(flippedPos, 1.0);
}
