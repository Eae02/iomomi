#version 450 core
#pragma variants V1 V2

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec2 depth_out;

layout(binding=0) uniform sampler2D depthSampler;

const float ZNear = 0.1;
const float ZFar = 1000.0;

#ifdef V1
layout(binding=1) uniform sampler2D travelDepthSampler;
float linearizeDepth(float hDepth)
{
	return 2.0 * ZNear * ZFar / (ZFar + ZNear - (hDepth * (ZFar - ZNear)));
}
#endif

const int FILTER_RADIUS = 32;

layout(push_constant) uniform PC
{
	vec2 blurDir;
	float blurScale;
	float blurDepthFalloff;
};

void main()
{
	float centerDepth = texture(depthSampler, texCoord_in).x;
#ifdef V1
	centerDepth = linearizeDepth(centerDepth);
#endif
	
	float blurDist = 1.0 / centerDepth;
	
	vec2 sum = vec2(0);
	float wsum = 0;
	for (int x = -FILTER_RADIUS; x <= FILTER_RADIUS; x++)
	{
		vec2 tc = texCoord_in + x * blurDir * blurDist;
		
#ifdef V1
		vec2 s = vec2(linearizeDepth(texture(depthSampler, tc).x), texture(travelDepthSampler, tc).x);
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
