#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in uvec3 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec3 texCoord_out;
layout(location=2) out vec3 normal_out;
layout(location=3) out vec3 tangent_out;

#include "../Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	worldPos_out = position_in;
	normal_out = normal_in;
	tangent_out = tangent_in;
	
	vec3 biTangent = cross(tangent_in, normal_in);
	texCoord_out = vec3(texCoord_in.xy / 255.0, texCoord_in.z);
	
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
