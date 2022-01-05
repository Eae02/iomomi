#version 450 core
#pragma variants VPass1 VPass2

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D inputSSRSampler;

#ifdef VPass2
layout(binding=1) uniform sampler2D litNoSSRColorSampler;
#endif

layout(push_constant) uniform PC
{
	vec2 blurDir;
	float maxBlurDist;
	vec4 blurCoeff[4];
};

layout(constant_id=0) const int FILTER_RADIUS = 16;

void main()
{
	vec4 centerSample = texture(inputSSRSampler, texCoord_in);
	vec2 scaledBlurDir = blurDir * centerSample.a;
	scaledBlurDir = min(abs(scaledBlurDir), vec2(maxBlurDist) / vec2(textureSize(inputSSRSampler, 0).xy));
	
	vec3 blurSum = centerSample.rgb * blurCoeff[0][0];
	for (int x = 1; x < FILTER_RADIUS; x++)
	{
		float w = blurCoeff[x/4][x%4];
		vec3 s1 = texture(inputSSRSampler, texCoord_in + float(x) * scaledBlurDir).rgb;
		vec3 s2 = texture(inputSSRSampler, texCoord_in - float(x) * scaledBlurDir).rgb;
		blurSum += (s1 + s2) * w;
	}
	
#ifdef VPass2
	color_out = texture(litNoSSRColorSampler, texCoord_in) + vec4(blurSum, 0);
#else
	color_out = vec4(blurSum, centerSample.a);
#endif
}
