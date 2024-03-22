#version 450 core
#pragma variants V1 V2

#include "WaterCommon.glh"
#include "../Inc/Depth.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec2 depths_out;

layout(binding=0) uniform Params_UseDynamicOffset
{
	vec2 blurDir;
	float blurScale;
	float blurDepthFalloff;
};

layout(binding=1) uniform sampler uSampler_UF;

layout(binding=2) uniform texture2D depthTex_UF;

#ifdef V1
layout(binding=3) uniform texture2D depthMaxTex_UF;
#endif

layout(constant_id=0) const int FILTER_RADIUS = 16;

const float BLUR_RAMP_BEGIN = 5;
const float BLUR_RAMP_SIZE = 3;
const float MAX_BLUR_RAMP = 1.0;

void main()
{
	vec4 centerSample = texture(sampler2D(depthTex_UF, uSampler_UF), texCoord_in);
	
#ifdef V1
	float centerDepth       = linearizeDepth(centerSample.x);
	float centerDepthMax    = linearizeDepth(texture(sampler2D(depthMaxTex_UF, uSampler_UF), texCoord_in).r);
#else
	float centerDepth       = centerSample.x;
	float centerDepthMax    = centerSample.y;
#endif
	
	//float centerTravelDepth = centerDepthMax - centerDepth;
	//float blurRamp = clamp((centerTravelDepth - BLUR_RAMP_BEGIN) / BLUR_RAMP_SIZE, 0, 1);
	float blurDist = (1) / (centerDepth < W_UNDERWATER_DEPTH ? centerDepthMax : centerDepth);
	
	vec2 sum = vec2(0);
	vec2 wsum = vec2(0);
	for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; x++)
	{
		vec2 tc = texCoord_in + x * blurDir * blurDist;
		
#ifdef V1
		vec2 s = vec2(
			linearizeDepth(texture(sampler2D(depthTex_UF, uSampler_UF), tc).x),
			linearizeDepth(texture(sampler2D(depthMaxTex_UF, uSampler_UF), tc).x)
		);
#else
		vec2 s = texture(sampler2D(depthTex_UF, uSampler_UF), tc).xy;
#endif
		
		float r = x * blurScale;
		float wDist = exp(-r * r);
		
		float rMin = abs(s.x - centerDepth) * blurDepthFalloff;
		float gMin = exp(-rMin * rMin);
		
		float rMax = abs(s.y - centerDepthMax) * blurDepthFalloff;
		float gMax = exp(-rMax * rMax);
		
		vec2 weight = wDist * vec2(gMin, gMax);
		sum += s * weight;
		wsum += weight;
	}
	
	depths_out = sum / max(wsum, vec2(0.001));
}
