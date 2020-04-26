#version 450 core

#pragma variants VDefault VPlanarRefl

layout(location=0) in vec3 position_in;
layout(location=1) in uvec4 misc_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec4 texCoord_out;
layout(location=1) out vec3 normal_out;
layout(location=2) out vec3 tangent_out;
layout(location=3) out vec4 ao_out;
layout(location=4) out vec2 roughnessRange_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(binding=1, std140) uniform MaterialSettingsUB
{
	vec4 matSettings[4]; 
};

#ifdef VPlanarRefl
layout(push_constant) uniform PC
{
	vec4 plane;
};

#define REFLECTION_CLIP_PLANE 1
#include "Inc/PlanarRefl.glh"
#endif

void main()
{
	normal_out = normal_in;
	tangent_out = tangent_in;
	
	vec3 biTangent = cross(tangent_in, normal_in);
	vec2 texCoord2 = vec2(dot(biTangent, position_in), -dot(tangent_in, position_in));
	texCoord_out = vec4(texCoord2, float(misc_in.x) - 1, 1.0);
	
	if (misc_in.x != 0)
	{
		vec4 mat = matSettings[misc_in.x - 1];
		texCoord_out.xy /= mat.x;
		texCoord_out.w = mat.x;
		roughnessRange_out = mat.yz;
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
	
	gl_ClipDistance[0] = misc_in.y / 127.0 - 1.0;
	
	vec3 worldPos = position_in;
#ifdef VPlanarRefl
	planarReflection(plane, worldPos);
#endif
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
