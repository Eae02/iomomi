#version 450 core
#pragma variants V1 V2

#include "../Inc/Depth.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec2 depth_out;

layout(binding=0) uniform sampler2D depthSampler;

#ifdef V1
layout(binding=1) uniform sampler2D travelDepthSampler;
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
const float MAX_BLUR_RAMP = 3.0;

void main()
{
	vec2 centerSample = texture(depthSampler, texCoord_in).rg;
	
#ifdef V1
	float centerDepth = linearizeDepth(depthTo01(centerSample.x));
	float centerTravelDepth = texture(travelDepthSampler, texCoord_in).x;
#else
	float centerDepth = centerSample.x;
	float centerTravelDepth = centerSample.y;
#endif
	
	float blurRamp = clamp((centerTravelDepth - BLUR_RAMP_BEGIN) / BLUR_RAMP_SIZE, 0, 1);
	float blurDist = (blurRamp * (MAX_BLUR_RAMP - 1) + 1) / centerDepth;
	
	vec2 sum = vec2(0);
	float wsum = 0;
	for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; x++)
	{
		vec2 tc = texCoord_in + x * blurDir * blurDist;
		
#ifdef V1
		vec2 s = vec2(linearizeDepth(depthTo01(texture(depthSampler, tc).x)), texture(travelDepthSampler, tc).x);
#else
		vec2 s = texture(depthSampler, tc).xy;
#endif
		
		float r = x * blurScale;
		float w = exp(-r * r);
		
		float r2 = abs(s.x - centerDepth) * blurDepthFalloff;
		float g = exp(-r2 * r2);
		
		sum += s * w * g;
		wsum += w * g;
	}
	
	if (wsum > 0.0)
		sum /= wsum;
	
	depth_out = sum;
}
