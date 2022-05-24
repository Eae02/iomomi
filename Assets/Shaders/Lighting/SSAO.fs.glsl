#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/Depth.glh"
#include "../Inc/RenderSettings.glh"
#include <Deferred.glh>

layout(binding=1) uniform sampler2D gbDepthLinSampler;
layout(binding=2) uniform sampler2D gbColor2Sampler;

layout(push_constant) uniform PC
{
	float sampleRadius;
	float depthFadeBegin;
	float depthFadeRate;
	float ssaoMax;
};

const int MAX_SAMPLES = 24;

layout(constant_id=0) const int numSamples = 24;
layout(constant_id=1) const float rotationsTexScale = 1;

layout(set=0, binding=3) uniform sampler2D rotationsTex;

layout(set=0, binding=4, std140) uniform SSAOData
{
	vec4 sampleVectors[MAX_SAMPLES];
};

layout(location=0) out float ssao_out;

layout(location=0) in vec2 screenCoord_in;

void main()
{
	float centerHDepth = hyperDepth(texture(gbDepthLinSampler, screenCoord_in).r);
	vec3 worldPos = WorldPosFromDepth(centerHDepth, screenCoord_in, renderSettings.invViewProjection);
	vec3 normal = SMDecode(texture(gbColor2Sampler, screenCoord_in).xy);
	
	vec2 rSinCos = texture(rotationsTex, vec2(gl_FragCoord.xy) * rotationsTexScale).xy;
	vec3 tangent = vec3(1, 0, 0);
	if (abs(normal.y) < 0.999)
		tangent = normalize(cross(normal, vec3(0, 1, 0)));
	mat3 tbn = mat3(tangent, normalize(cross(tangent, normal)), normal);
	
	float ssao = 0;
	for (int i = 0; i < numSamples; i++)
	{
		vec3 sv = sampleRadius * sampleVectors[i].xyz;
		vec3 samplePos = worldPos + tbn * vec3(
			sv.x * rSinCos.x - sv.y * rSinCos.y,
			sv.x * rSinCos.y + sv.y * rSinCos.x,
			sv.z
		);
		
		vec4 samplePos4 = renderSettings.viewProjection * vec4(samplePos, 1.0);
		vec3 samplePosNDC = samplePos4.xyz / samplePos4.w;
		float samplePosDepth = linearizeDepth(depthTo01(samplePosNDC.z));
		
		vec2 sampleTC = samplePosNDC.xy * 0.5 + 0.5;
		if (!EG_OPENGL)
			sampleTC.y = 1 - sampleTC.y;
		
		float sampledDepth = texture(gbDepthLinSampler, sampleTC).r;
		if (sampledDepth < samplePosDepth)
		{
			ssao += clamp(1 - (samplePosDepth - sampledDepth - depthFadeBegin) * depthFadeRate, 0, 1);
		}
	}
	ssao = 1.0 - ssao / float(numSamples);
	float ssaoSquared = ssao * ssao;
	ssao_out = min(ssaoSquared * ssaoSquared * ssaoSquared * ssaoMax, 1);
}
