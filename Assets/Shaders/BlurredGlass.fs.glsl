#version 450 core

#pragma variants VNoBlur VBlur

layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(set=0, binding=1) uniform texture2D hexTexture;

#ifdef VBlur
layout(set=1, binding=0) uniform texture2D blurTexture;
#endif

#ifdef VNoBlur
layout(set=1, binding=0) uniform texture2D blurredGlassDepth_UF;
#endif

#define RENDER_SETTINGS_BINDING 0
#include "Inc/RenderSettings.glh"

layout(set=0, binding=2) uniform PC
{
	vec4 glassColor;
} pc;

layout(set=0, binding=3) uniform sampler samplerCommon;
layout(set=0, binding=4) uniform sampler samplerLinearClamp;
layout(set=0, binding=5) uniform sampler samplerNearestClamp;

const float LOW_ALPHA = 0.5;

void main()
{
	float hex = texture(sampler2D(hexTexture, samplerCommon), texCoord_in * 2).r;
	
#ifdef VBlur
	vec2 screenCoord = vec2(gl_FragCoord.xy) * renderSettings.inverseRenderResolution;
	vec3 lowBlur = textureLod(sampler2D(blurTexture, samplerLinearClamp), screenCoord, 0).rgb;
	vec3 highBlur = textureLod(sampler2D(blurTexture, samplerLinearClamp), screenCoord, 3).rgb;
	vec3 blurColor = mix(lowBlur, highBlur, hex);
	color_out = vec4(pc.glassColor.rgb * blurColor, 1);
#endif
	
#ifdef VNoBlur
	vec2 screenCoord = vec2(gl_FragCoord.xy) * renderSettings.inverseRenderResolution;
	if (gl_FragCoord.z <= texture(sampler2D(blurredGlassDepth_UF, samplerNearestClamp), screenCoord).r + 1E-4)
		discard;
	
	color_out = vec4(pc.glassColor.rgb * mix(1, pc.glassColor.a, hex), 1);
#endif
}
