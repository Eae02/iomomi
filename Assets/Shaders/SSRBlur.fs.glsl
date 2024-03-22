#version 450 core
#pragma variants VPass1 VPass2

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(set=1, binding=0) uniform sampler linearSampler;

layout(set=1, binding=1) uniform texture2D inputSSRTex;

#ifdef VPass2
layout(set=1, binding=2) uniform texture2D litNoSSRColorTex;
#endif

layout(set=0, binding=0) uniform Params
{
	float blurDist;
	float maxBlurDist;
	vec4 blurCoeff[4];
};

layout(constant_id=0) const int FILTER_RADIUS = 16;

void main()
{
	vec4 centerSample = texture(sampler2D(inputSSRTex, linearSampler), texCoord_in);
	
	vec2 texSize = vec2(textureSize(sampler2D(inputSSRTex, linearSampler), 0).xy);
	
	float scaledBlurDist = blurDist * centerSample.a;
#ifdef VPass2
	scaledBlurDist *= texSize.y / texSize.x;
	vec2 scaledBlurDir = vec2(0, scaledBlurDist);
#else
	vec2 scaledBlurDir = vec2(scaledBlurDist, 0);
#endif
	
	scaledBlurDir = min(abs(scaledBlurDir), vec2(maxBlurDist) / texSize);
	
	vec3 blurSum = centerSample.rgb * blurCoeff[0][0];
	for (int x = 1; x < FILTER_RADIUS; x++)
	{
		float w = blurCoeff[x/4][x%4];
		vec3 s1 = texture(sampler2D(inputSSRTex, linearSampler), texCoord_in + float(x) * scaledBlurDir).rgb;
		vec3 s2 = texture(sampler2D(inputSSRTex, linearSampler), texCoord_in - float(x) * scaledBlurDir).rgb;
		blurSum += (s1 + s2) * w;
	}
	
#ifdef VPass2
	color_out = texture(sampler2D(litNoSSRColorTex, linearSampler), texCoord_in) + vec4(blurSum, 0);
#else
	color_out = vec4(blurSum, centerSample.a);
#endif
}
