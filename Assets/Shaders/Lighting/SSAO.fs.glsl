#version 450 core

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/Depth.glh"
#include "../Inc/RenderSettings.glh"
#include <Deferred.glh>

const int MAX_SAMPLES = 24;

layout(set = 0, binding = 1, std140) uniform SSAOData
{
	float sampleRadius;
	float depthFadeBegin;
	float depthFadeRate;
	float ssaoMax;
	vec4 sampleVectors[MAX_SAMPLES];
};

layout(set = 0, binding = 2) uniform texture2D rotationsTex;

layout(set = 0, binding = 3) uniform sampler linearRepeatSampler;
layout(set = 0, binding = 4) uniform sampler linearClampSampler;

layout(set = 1, binding = 0) uniform texture2D gbDepthLinTex;
layout(set = 1, binding = 1) uniform texture2D gbColor2Tex;

layout(constant_id = 0) const int numSamples = 24;
layout(constant_id = 1) const float rotationsTexScale = 1;

layout(location = 0) out float ssao_out;

layout(location = 0) in vec2 screenCoord_in;

void main()
{
	float centerHDepth = hyperDepth(texture(sampler2D(gbDepthLinTex, linearClampSampler), screenCoord_in).r);
	vec3 worldPos = WorldPosFromDepth(centerHDepth, screenCoord_in, renderSettings.invViewProjection);
	vec3 normal = SMDecode(texture(sampler2D(gbColor2Tex, linearClampSampler), screenCoord_in).xy); //TODO Texel fetch?

	vec2 rSinCos = texture(sampler2D(rotationsTex, linearRepeatSampler), vec2(gl_FragCoord.xy) * rotationsTexScale).xy;
	vec3 tangent = vec3(1, 0, 0);
	if (abs(normal.y) < 0.999)
		tangent = normalize(cross(normal, vec3(0, 1, 0)));
	mat3 tbn = mat3(tangent, normalize(cross(tangent, normal)), normal);

	float ssao = 0;
	for (int i = 0; i < numSamples; i++)
	{
		vec3 sv = sampleRadius * sampleVectors[i].xyz;
		vec3 samplePos =
			worldPos + tbn * vec3(sv.x * rSinCos.x - sv.y * rSinCos.y, sv.x * rSinCos.y + sv.y * rSinCos.x, sv.z);

		vec4 samplePos4 = renderSettings.viewProjection * vec4(samplePos, 1.0);
		vec3 samplePosNDC = samplePos4.xyz / samplePos4.w;
		float samplePosDepth = linearizeDepth(depthTo01(samplePosNDC.z));

		vec2 sampleTC = samplePosNDC.xy * 0.5 + 0.5;
		if (!EG_OPENGL)
			sampleTC.y = 1 - sampleTC.y;

		float sampledDepth = texture(sampler2D(gbDepthLinTex, linearClampSampler), sampleTC).r;
		if (sampledDepth < samplePosDepth)
		{
			ssao += clamp(1 - (samplePosDepth - sampledDepth - depthFadeBegin) * depthFadeRate, 0, 1);
		}
	}
	ssao = 1.0 - ssao / float(numSamples);
	float ssaoSquared = ssao * ssao;
	ssao_out = min(ssaoSquared * ssaoSquared * ssaoSquared * ssaoMax, 1);
}
