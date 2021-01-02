#version 450 core
#pragma variants V1 V2

#include "WaterCommon.glh"
#include "../Inc/Depth.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 depth_out;

layout(binding=0) uniform sampler2D depthSampler;

#ifdef V1
layout(binding=1) uniform sampler2D travelDepthSampler;
layout(binding=2) uniform sampler2D depthMaxSampler;
layout(binding=3) uniform sampler2D glowIntensitySampler;
#endif

layout(constant_id=0) const int FILTER_RADIUS = 16;

layout(push_constant) uniform PC
{
	vec2 blurDir;
	float blurScale;
	float blurDepthFalloff;
};

const float BLUR_RAMP_BEGIN = 5;
const float BLUR_RAMP_SIZE = 3;
const float MAX_BLUR_RAMP = 1.0;

void main()
{
	vec4 centerSample = texture(depthSampler, texCoord_in);
	
#ifdef V1
	float centerDepth       = linearizeDepth(depthTo01(centerSample.x));
	float centerTravelDepth = texture(travelDepthSampler, texCoord_in).x;
	float centerDepthMax    = linearizeDepth(depthTo01(texture(depthMaxSampler, texCoord_in).r));
	float glowIntensity     = texture(glowIntensitySampler, texCoord_in).r;
#else
	float centerDepth       = centerSample.x;
	float centerTravelDepth = centerSample.y;
	float centerDepthMax    = centerSample.z;
	float glowIntensity     = centerSample.w;
#endif
	
	float blurRamp = clamp((centerTravelDepth - BLUR_RAMP_BEGIN) / BLUR_RAMP_SIZE, 0, 1);
	float blurDist = (blurRamp * (MAX_BLUR_RAMP - 1) + 1) / (centerDepth < UNDERWATER_DEPTH ? centerDepthMax : centerDepth);
	
	vec3 sum = vec3(0);
	vec3 wsum = vec3(0);
	for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; x++)
	{
		vec2 tc = texCoord_in + x * blurDir * blurDist;
		
#ifdef V1
		vec3 s = vec3(
			linearizeDepth(depthTo01(texture(depthSampler, tc).x)),
			texture(travelDepthSampler, tc).x,
			linearizeDepth(depthTo01(texture(depthMaxSampler, tc).x))
		);
#else
		vec3 s = texture(depthSampler, tc).xyz;
#endif
		
		float r = x * blurScale;
		float wDist = exp(-r * r);
		
		float rMin = abs(s.x - centerDepth) * blurDepthFalloff;
		float gMin = exp(-rMin * rMin);
		
		float rMax = abs(s.z - centerDepthMax) * blurDepthFalloff;
		float gMax = exp(-rMax * rMax);
		
		vec3 weight = wDist * vec3(gMin, gMin, gMax);
		sum += s * weight;
		wsum += weight;
	}
	
	sum /= max(wsum, vec3(0.001));
	
	depth_out = vec4(sum, glowIntensity);
}
