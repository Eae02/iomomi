#version 420 core

layout(location=0) in vec2 position_in;
layout(location=1) in vec4 in0;
layout(location=2) in vec4 in1;
layout(location=3) in vec4 in2;

layout(location=0) out vec2 texCoord_out;
layout(location=1) out float opacity_out;
layout(location=2) flat out int additive_out;

#include "Inc/RenderSettings.glh"

layout(binding=0, std140) uniform RenderSettingsUB
{
	RenderSettings renderSettings;
};

void main()
{
	vec3 toCamera = renderSettings.cameraPosition - in0.xyz;
	vec3 left = normalize(cross(toCamera, vec3(0, 1, 0)));
	vec3 up = normalize(cross(left, toCamera));
	
	vec2 sinCos = in2.xy * (510.0 / 254.0) - 1.0;
	mat2 rotationMatrixL = mat2(sinCos.y, -sinCos.x, sinCos.x, sinCos.y);
	mat2x3 rotationMatrixW = mat2x3(left, up);
	vec2 localPos = (rotationMatrixL * (position_in * 2 - 1)) * in0.w;
	vec3 worldPos = in0.xyz + left * localPos.x + up * localPos.y;
	
	texCoord_out = mix(in1.xy, in1.zw, position_in);
	opacity_out = in2.z;
	additive_out = (in2.w > 0.5) ? 1 : 0;
	
	gl_Position = renderSettings.viewProjection * vec4(worldPos, 1.0);
}
