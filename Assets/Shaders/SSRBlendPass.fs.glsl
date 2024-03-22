#version 450 core

#include <Deferred.glh>
#define RENDER_SETTINGS_BINDING 0
#include "Inc/Depth.glh"
#include "Inc/GBData.glh"
#include "Inc/Light.glh"
#include "Inc/RenderSettings.glh"

layout(location = 0) in vec2 texCoord_in;

layout(location = 0) out vec4 color_out;

layout(set = 1, binding = 0) uniform texture2D gbColor1Tex;
layout(set = 1, binding = 1) uniform texture2D gbColor2Tex;
layout(set = 1, binding = 2) uniform texture2D gbDepthTex_UF;

layout(set = 1, binding = 3) uniform sampler nearestSampler_UF;

layout(set = 0, binding = 1) uniform Params
{
	vec3 fallbackColor;
	float ssrIntensity;
};

void main()
{
	float hDepth = texture(sampler2D(gbDepthTex_UF, nearestSampler_UF), texCoord_in).r;
	vec4 gbc1 = texture(sampler2D(gbColor1Tex, nearestSampler_UF), texCoord_in);
	vec4 gbc2 = texture(sampler2D(gbColor2Tex, nearestSampler_UF), texCoord_in);

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
