#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec3 texCoord_in;
layout(location=2) in vec4 normalAndRoughnessLo_in;
layout(location=3) in vec4 tangentAndRoughnessHi_in;

layout(location=0) out vec3 texCoord_out;
layout(location=1) out vec3 normal_out;
layout(location=2) out vec3 tangent_out;
layout(location=3) out vec2 roughnessRange_out;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	normal_out = normalAndRoughnessLo_in.xyz;
	tangent_out = tangentAndRoughnessHi_in.xyz;
	texCoord_out = texCoord_in;
	roughnessRange_out = vec2(normalAndRoughnessLo_in.w, tangentAndRoughnessHi_in.w) * 0.5 + 0.5;
	gl_Position = renderSettings.viewProjection * vec4(position_in, 1.0);
}
