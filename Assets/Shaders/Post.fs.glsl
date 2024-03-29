#version 450 core

layout(constant_id=0) const int enableFXAA = 0;

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D inputSampler;

layout(binding=1) uniform sampler2D bloomSampler;

layout (push_constant) uniform PC
{
	float exposure;
	float bloomIntensity;
	float finalColorScale;
	float vignetteRadMin;
	float vignetteRadScale;
	float vignettePower;
};

vec3 doFXAA()
{
	vec3 luma = vec3(0.299, 0.587, 0.114);
	
	float SPAN_MAX = 8;
	float REDUCE_MIN = 1 / 128.0;
	float REDUCE_MUL = 1 / 8.0;
	
	float lumaTL = dot(luma, textureOffset(inputSampler, texCoord_in, ivec2(-1, -1)).xyz);
	float lumaTR = dot(luma, textureOffset(inputSampler, texCoord_in, ivec2(1, -1)).xyz);
	float lumaBL = dot(luma, textureOffset(inputSampler, texCoord_in, ivec2(-1, 1)).xyz);
	float lumaBR = dot(luma, textureOffset(inputSampler, texCoord_in, ivec2(1, 1)).xyz);
	float lumaMid = dot(luma, texture(inputSampler, texCoord_in).xyz);
	
	vec2 dir = vec2(0, 0);
	dir.x = -((lumaTL + lumaTR) - (lumaBL + lumaBR));
	dir.y = (lumaTL + lumaBL) - (lumaTR + lumaBR);
	
	float dirReduce = max((lumaTL + lumaTR + lumaBL + lumaBR) * (REDUCE_MUL * 0.25), REDUCE_MIN);
	float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	
	dir = min(vec2(SPAN_MAX, SPAN_MAX), max(vec2(-SPAN_MAX, -SPAN_MAX), dir * scale)) / vec2(textureSize(inputSampler, 0));
	
	vec3 result1 = 0.5 * (
		texture(inputSampler, texCoord_in + (dir * vec2(1 / 3.0 - 0.5))).xyz + 
		texture(inputSampler, texCoord_in + (dir * vec2(2 / 3.0 - 0.5))).xyz
	);
	
	vec3 result2 = result1 * 0.5 + 0.25 * (
		texture(inputSampler, texCoord_in + (dir * vec2(0 / 3.0 - 0.5))).xyz + 
		texture(inputSampler, texCoord_in + (dir * vec2(3 / 3.0 - 0.5))).xyz
	);
	
	float lumaMin = min(lumaMid, min(min(lumaTL, lumaTR), min(lumaBL, lumaBR)));
	float lumaMax = min(lumaMid, max(max(lumaTL, lumaTR), max(lumaBL, lumaBR)));
	float lumaResult2 = dot(luma, result2);
	
	if (lumaResult2 < lumaMin || lumaResult2 > lumaMax)
		return result1;
	return result2;
}

void main()
{
	vec3 color;
	if (enableFXAA != 0)
		color = doFXAA();
	else
		color = texture(inputSampler, texCoord_in).rgb;
	
	color += texture(bloomSampler, texCoord_in).rgb * bloomIntensity;
	
	float vignette = pow(clamp((length(texCoord_in - 0.5) - vignetteRadMin) * vignetteRadScale, 0, 1), vignettePower);
	color_out = vec4((vec3(1.0) - exp(-exposure * color)) * (1.0 - vignette) * finalColorScale, 1.0);
}
