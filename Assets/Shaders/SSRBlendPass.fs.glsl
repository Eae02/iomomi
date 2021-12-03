#version 450 core

#include <Deferred.glh>
#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/GBData.glh"
#include "Inc/Depth.glh"
#include "Inc/Light.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D gbColor1Sampler;
layout(binding=2) uniform sampler2D gbColor2Sampler;
layout(binding=3) uniform sampler2D gbDepthSampler;

layout(push_constant) uniform PC
{
	float ssrIntensity;
};

void main()
{
	float hDepth = texture(gbDepthSampler, texCoord_in).r;
	vec4 gbc1 = texture(gbColor1Sampler, texCoord_in);
	vec4 gbc2 = texture(gbColor2Sampler, texCoord_in);
	
	GBData data;
	data.hDepth = hDepth;
	data.worldPos = WorldPosFromDepth(hDepth, texCoord_in, renderSettings.invViewProjection);
	data.normal = SMDecode(gbc2.xy);
	data.albedo = gbc1.xyz;
	data.roughness = gbc2.z;
	data.metallic = gbc2.w;
	data.ao = gbc1.w;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - data.worldPos);
	vec3 fresnel = calcFresnel(data, toEye);
	
	color_out.xyz = fresnel * (1 - data.roughness) * ssrIntensity;
	color_out.a = data.roughness / distance(data.worldPos, renderSettings.cameraPosition);
}
