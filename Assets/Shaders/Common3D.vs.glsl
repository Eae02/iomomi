#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;
layout(location=4) in mat3x4 worldTransform_in;
layout(location=7) in vec2 textureScale_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec2 texCoord_out;
layout(location=2) out vec3 normal_out;
layout(location=3) out vec3 tangent_out;

layout(constant_id=151) const float depthOffset = 0;

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

void main()
{
	mat4 worldTransform = transpose(mat4(worldTransform_in));
	
	worldPos_out = (worldTransform * vec4(position_in, 1.0)).xyz;
	texCoord_out = textureScale_in * texCoord_in;
	normal_out = normalize((worldTransform * vec4(normal_in, 0.0)).xyz);
	tangent_out = normalize((worldTransform * vec4(tangent_in, 0.0)).xyz);
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos_out, 1.0);
	gl_Position.z -= depthOffset;
}
