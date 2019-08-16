#version 450 core

#include "../Inc/Depth.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec2 depth_out;

layout(binding=0) uniform sampler2D depthSampler;
layout(binding=1) uniform sampler2D travelDepthSampler;

layout(constant_id=0) const int FILTER_RADIUS = 16;

layout(push_constant) uniform PC
{
	vec2 blurDir;
	float blurScale;
	float blurDepthFalloff;
};

void main()
{
	float centerDepth = linearizeDepth(depthTo01(texture(depthSampler, texCoord_in).x));
	
	float blurDist = 1.0 / centerDepth;
	
	vec2 sum = vec2(0);
	float wsum = 0;
	for (int y = -FILTER_RADIUS; y <= FILTER_RADIUS; y++)
	{
		for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; x++)
		{
			vec2 relCoord = vec2(x, y);
			vec2 tc = texCoord_in + relCoord * blurDir * blurDist;
			
			vec2 s = vec2(linearizeDepth(depthTo01(texture(depthSampler, tc).x)), texture(travelDepthSampler, tc).x);
			
			float r = length(relCoord) * blurScale;
			float w = exp(-r * r);
			
			float r2 = abs(s.x - centerDepth) * blurDepthFalloff;
			float g = exp(-r2 * r2);
			
			sum += s * w * g;
			wsum += w * g;
		}
	}
	
	if (wsum > 0.0)
		sum /= wsum;
	
	depth_out = sum;
}
