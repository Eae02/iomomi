#version 450 core

#pragma variants VNoBlur VBlur

layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D hexTexture;

#ifdef VBlur
layout(binding=2) uniform sampler2D blurTexture;
#endif

#ifdef VNoBlur
layout(binding=2) uniform sampler2D blurredGlassDepth;
#endif

layout(push_constant) uniform PC
{
	vec4 glassColor;
#ifdef VBlur
	vec2 pixelSize;
#endif
} pc;

const float LOW_ALPHA = 0.5;

void main()
{
	float hex = texture(hexTexture, texCoord_in * 2).r;
	
#ifdef VBlur
	vec2 screenCoord = vec2(gl_FragCoord.xy) * pc.pixelSize;
	vec3 lowBlur = textureLod(blurTexture, screenCoord, 0).rgb;
	vec3 highBlur = textureLod(blurTexture, screenCoord, 3).rgb;
	vec3 blurColor = mix(lowBlur, highBlur, hex);
	color_out = vec4(pc.glassColor.rgb * blurColor, 1);
#endif
	
#ifdef VNoBlur
	vec2 screenCoord = ivec2(gl_FragCoord.xy) / vec2(textureSize(blurredGlassDepth, 0).xy);
	if (gl_FragCoord.z <= texture(blurredGlassDepth, screenCoord).r + 1E-4)
		discard;
	
	color_out = vec4(pc.glassColor.rgb * mix(1, pc.glassColor.a, hex), 1);
#endif
}
