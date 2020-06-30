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
layout(binding=5) uniform usampler2D gbFlagsSampler;
layout(binding=6) uniform sampler2D waterDepthSampler;

layout(constant_id=0) const int ssrLinearSamples = 8;
layout(constant_id=1) const int ssrBinarySamples = 8;

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
	if (ndc.x < -1 || ndc.y < -1 || ndc.x > 1 || ndc.y > 1)
		return true;
	
	sampleTC = ndc.xy * 0.5 + 0.5;
	if (!EG_OPENGL)
		sampleTC.y = 1 - sampleTC.y;
	
	return depthTo01(ndc.z) > min(texture(gbDepthSampler, sampleTC).r, getWaterHDepth(sampleTC));
}

vec3 calcReflection(vec3 surfacePos, vec3 dirToEye, vec3 normal)
{
	surfacePos += normal * 0.01;
	
	const float MAX_DIST   = 20;   //Maximum distance to reflection
	const float FADE_BEGIN = 0.75; //Percentage of screen radius to begin fading out at
	
	vec3 rayDir = normalize(reflect(-dirToEye, normal)) * (MAX_DIST / ssrLinearSamples);
	
	for (uint i = 1; i <= ssrLinearSamples; i++)
	{
		vec3 sampleWorldPos = surfacePos + rayDir * i;
		if (behindDepthBuffer(sampleWorldPos))
		{
			float lo = i - 1;
			float hi = i;
			for (int i = 0; i < ssrBinarySamples; i++)
			{
				float mid = (lo + hi) / 2.0;
				if (behindDepthBuffer(surfacePos + rayDir * mid))
					hi = mid;
				else
					lo = mid;
			}
			
			float fade01 = max(abs(ndc.x), abs(ndc.y));
			float fade = 1 - clamp((fade01 - 1) / (1 - FADE_BEGIN) + 1, 0, 1);
			return texture(inputColorSampler, sampleTC).rgb * fade;
		}
	}
	
	return vec3(0.0);
}

layout(location=0) out vec4 color_out;

void main()
{
	uint flags = texelFetch(gbFlagsSampler, ivec2(gl_FragCoord.xy), 0).r;
	
	color_out = texture(inputColorSampler, texCoord_in);
	
	if ((flags & (RF_NO_SSR | RF_NO_LIGHTING)) != 0)
		return;
	
	float hDepth = texture(gbDepthSampler, texCoord_in).r;;
	if (getWaterHDepth(texCoord_in) < hDepth)
		return;
	
	vec4 gbc1 = texture(gbColor1Sampler, texCoord_in);
	vec4 gbc2 = texture(gbColor2Sampler, texCoord_in);
	
	GBData data;
	data.flags = flags;
	data.hDepth = hDepth;
	data.worldPos = WorldPosFromDepth(hDepth, texCoord_in, renderSettings.invViewProjection);
	data.normal = SMDecode(gbc2.xy);
	data.albedo = gbc1.xyz;
	data.roughness = gbc2.z;
	data.metallic = gbc2.w;
	data.ao = gbc1.w;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - data.worldPos);
	vec3 fresnel = calcFresnel(data, toEye);
	vec3 reflection = calcReflection(data.worldPos, toEye, data.normal);
	
	color_out.rgb += reflection * fresnel * (1 - data.roughness) * data.ao;
}
