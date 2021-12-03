#version 450 core

#include <Deferred.glh>
#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"
#include "Inc/GBData.glh"
#include "Inc/Depth.glh"
#include "Inc/Light.glh"

layout(location=0) in vec2 texCoord_in;

layout(binding=1) uniform sampler2D inputColorSampler;
layout(binding=2) uniform sampler2D gbColor2Sampler;
layout(binding=3) uniform sampler2D gbDepthSampler;
layout(binding=4) uniform sampler2D waterDepthSampler;

layout(constant_id=0) const int ssrLinearSamples = 8;
layout(constant_id=1) const int ssrBinarySamples = 8;
layout(constant_id=2) const float ssrMaxDistance = 20;

layout(push_constant) uniform PC
{
	vec3 fallbackColor;
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

const float FADE_BEGIN = 0.75; //Percentage of screen radius to begin fading out at

vec4 calcReflection(vec3 surfacePos, vec3 dirToEye, vec3 normal)
{
	surfacePos += normal * 0.01;
	
	float rayDirLen = ssrMaxDistance / ssrLinearSamples;
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
			vec3 reflectColor = mix(fallbackColor, texture(inputColorSampler, sampleTC).rgb, fade);
			return vec4(reflectColor, mid * rayDirLen / ssrMaxDistance);
		}
	}
	
	return vec4(fallbackColor, 1);
}

layout(location=0) out vec4 color_out;

void main()
{
	gl_FragDepth = 0;
	color_out = vec4(0.0);
	
	float hDepth = texture(gbDepthSampler, texCoord_in).r;
	if (getWaterHDepth(texCoord_in) < hDepth)
		return;
	
	vec4 gbc2 = texture(gbColor2Sampler, texCoord_in);
	
	vec3 worldPos = WorldPosFromDepth(hDepth, texCoord_in, renderSettings.invViewProjection);
	vec3 normal = SMDecode(gbc2.xy);
	
	vec3 toEye = normalize(renderSettings.cameraPosition - worldPos);
	color_out = calcReflection(worldPos, toEye, normal);
	gl_FragDepth = color_out.a;
}
