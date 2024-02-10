#version 450 core

#define GB_WITH_BARRIER_SETTINGS_UB
#include "GBCommon.glh"

layout(location=0) in vec2 texCoord_in;
layout(location=1) in vec3 worldPos_in;

layout(location=0) out vec4 color_out;

layout(set=1, binding=0) uniform sampler2D waterDepth;
layout(set=1, binding=1) uniform sampler2D blurredGlassDepth;
layout(set=1, binding=2) uniform sampler2D waterDistanceTex;

#define WATER_DEPTH_OFFSET 0.15
#include "../Water/WaterTransparent.glh"

void main()
{
	if (CheckWaterDiscard())
	{
		color_out = vec4(0);
		return;
	}
	
	vec2 screenCoord = ivec2(gl_FragCoord.xy) / vec2(textureSize(blurredGlassDepth, 0).xy);
	if (gl_FragCoord.z < textureLod(blurredGlassDepth, screenCoord, 0).r)
		discard;
	
	vec2 scaledTC = texCoord_in * vec2(pc.tangent.w, pc.bitangent.w);
	float edgeDist = calcEdgeDist(scaledTC);
	float negScale = getInteractablesNegScale(worldPos_in, scaledTC, edgeDist, texture(waterDistanceTex, texCoord_in).r);
	
	color_out = calculateColor(scaledTC, edgeDist, negScale, 0);
}
