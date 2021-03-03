#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/Depth.glh"
#include "../Inc/RenderSettings.glh"
#include <Deferred.glh>

layout(binding=1) uniform sampler2D gbDepthSampler;
layout(binding=2) uniform sampler2D gbColor2Sampler;

layout(push_constant) uniform PC
{
	float sampleRadius;
	float depthFadeBegin;
	float depthFadeRate;
	float power;
};

const int MAX_SAMPLES = 24;

layout(constant_id=0) const int numSamples = 24;
layout(constant_id=1) const float rotationsTexScale = 1;

layout(set=0, binding=3, std140) uniform SSAOData
{
	vec4 sampleVectors[MAX_SAMPLES];
};

layout(set=0, binding=4) uniform sampler2D rotationsTex;

layout(location=0) out float ssao_out;

void main()
{
	vec2 screenCoord01 = vec2(gl_FragCoord.xy) / vec2(textureSize(gbColor2Sampler, 0).xy);
	
	float centerHDepth = texelFetch(gbDepthSampler, ivec2(gl_FragCoord.xy), 0).r;
	vec3 worldPos = WorldPosFromDepth(centerHDepth, screenCoord01, renderSettings.invViewProjection);
	vec3 normal = SMDecode(texelFetch(gbColor2Sampler, ivec2(gl_FragCoord.xy), 0).xy);
	
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
		float samplePosDepthH = depthTo01(samplePosNDC.z);
		
		vec2 sampleTC = samplePosNDC.xy * 0.5 + 0.5;
		if (!EG_OPENGL)
			sampleTC.y = 1 - sampleTC.y;
		
		float sampledDepthH = texture(gbDepthSampler, sampleTC).r;
		if (sampledDepthH < samplePosDepthH)
		{
			float depthDiff = linearizeDepth(samplePosDepthH) - linearizeDepth(sampledDepthH);
			ssao += clamp(1 - (depthDiff - depthFadeBegin) * depthFadeRate, 0, 1);
		}
	}
	ssao_out = pow(1 - ssao / float(numSamples), power);
}
