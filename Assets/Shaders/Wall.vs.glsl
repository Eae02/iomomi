#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in uvec4 texCoordAO_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 texCoord_out;
layout(location=1) out vec3 normal_out;
layout(location=2) out vec3 tangent_out;
layout(location=3) out vec4 ao_out;
layout(location=4) out vec2 roughnessRange_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

layout(binding=1, std140) uniform MaterialSettingsUB
{
	vec4 matSettings[2]; 
};

void main()
{
	normal_out = normal_in;
	tangent_out = tangent_in;
	
	vec3 biTangent = cross(tangent_in, normal_in);
	vec2 texCoord2 = vec2(dot(biTangent, position_in), -dot(tangent_in, position_in));
	texCoord_out = vec4(texCoord2, float(texCoordAO_in.z) - 1, 1.0);
	
	if (texCoordAO_in.z != 0)
	{
		vec4 mat = matSettings[texCoordAO_in.z - 1];
		texCoord_out.xy /= mat.x;
		texCoord_out.w = mat.x;
		roughnessRange_out = mat.yz;
	}
	
	const float AO_BIAS = 0.30; //Increasing this makes the maximum amount of AO less intense.
	const float AO_SCALE = 3.5; //Decreasing this makes AO continue further from walls.
	
	vec4 ao;
	for (int i = 0; i < 4; i++)
		ao[i] = 1 - (texCoordAO_in.w >> i) & 1U;
	ao_out = ao * AO_SCALE + AO_BIAS;
	
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
