#version 450 core

#include <Deferred.glh>
#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/GBData.glh"
#include "Inc/Depth.glh"
#include "Inc/Light.glh"

layout(location=0) in vec2 texCoord_in;

layout(binding=1) uniform sampler2D inputColorSampler;
layout(binding=2) uniform sampler2D gbColor1Sampler;
layout(binding=3) uniform sampler2D gbColor2Sampler;
layout(binding=4) uniform sampler2D gbDepthSampler;
layout(binding=5) uniform sampler2D waterDepthSampler;

layout(constant_id=0) const int ssrLinearSamples = 8;
layout(constant_id=1) const int ssrBinarySamples = 8;
layout(constant_id=2) const int isDirect = 1;

layout(push_constant) uniform PC
{
	vec3 fallbackColor;
	float ssrIntensity;
};

float getWaterHDepth(vec2 texCoord)
{
	return hyperDepth(texture(waterDepthSampler, texCoord).r);
}

vec3 ndc;
vec2 sampleTC;
bool behindDepthBuffer(vec3 worldPos)
{
	vec4 ndc3 = renderSettings.viewProjection * vec4(worldPos, 1.0);
	ndc = ndc3.xyz / ndc3.w;
	
	sampleTC = ndc.xy * 0.5 + 0.5;
	if (!EG_OPENGL)
		sampleTC.y = 1 - sampleTC.y;
	
	return depthTo01(ndc.z) > min(texture(gbDepthSampler, sampleTC).r, getWaterHDepth(sampleTC));
}

vec4 calcReflection(vec3 surfacePos, vec3 dirToEye, vec3 normal)
{
	surfacePos += normal * 0.01;
	
	const float MAX_DIST   = 20;   //Maximum distance to reflection
	const float FADE_BEGIN = 0.75; //Percentage of screen radius to begin fading out at
	
	float rayDirLen = MAX_DIST / ssrLinearSamples;
	vec3 rayDir = normalize(reflect(-dirToEye, normal)) * rayDirLen;
	
	for (uint i = 1; i <= ssrLinearSamples; i++)
	{
		vec3 sampleWorldPos = surfacePos + rayDir * i;
		if (behindDepthBuffer(sampleWorldPos))
		{
			float lo = i - 1;
			float hi = i;
			float mid;
			for (int i = 0; i < ssrBinarySamples; i++)
			{
				mid = (lo + hi) / 2.0;
				if (behindDepthBuffer(surfacePos + rayDir * mid))
					hi = mid;
				else
					lo = mid;
			}
			
			float fade01 = max(abs(ndc.x), abs(ndc.y));
			float fade = 1 - clamp((fade01 - 1) / (1 - FADE_BEGIN) + 1, 0, 1);
			return vec4(mix(fallbackColor, texture(inputColorSampler, sampleTC).rgb, fade), mid * rayDirLen);
		}
	}
	
	return vec4(fallbackColor, MAX_DIST);
}

layout(location=0) out vec4 color_out;

void main()
{
	color_out = vec4(0.0);
	
	if (isDirect == 1)
	{
		color_out = texture(inputColorSampler, texCoord_in);
	}
	
	float hDepth = texture(gbDepthSampler, texCoord_in).r;;
	if (getWaterHDepth(texCoord_in) < hDepth)
		return;
	
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
	vec4 reflection = calcReflection(data.worldPos, toEye, data.normal);
	vec3 ssrColor = reflection.rgb * fresnel * (1 - data.roughness) * ssrIntensity;
	
	if (isDirect == 1)
	{
		color_out += vec4(ssrColor, 0);
	}
	else
	{
		float blur = reflection.a * data.roughness / distance(data.worldPos, renderSettings.cameraPosition);
		color_out = vec4(ssrColor, blur);
	}
}
