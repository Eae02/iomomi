#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

#include <EGame.glh>

#define GB_WITH_BARRIER_SETTINGS_UB
#include "GBCommon.glh"

layout(location = 0) in vec2 texCoord_in;
layout(location = 1) in vec3 worldPos_in;

layout(location = 0) out vec4 color_out;

layout(set = 1, binding = 1) uniform texture2D waterDistanceTex;
layout(set = 1, binding = 2) uniform sampler waterDistanceSampler;

layout(set = 2, binding = 0) uniform texture2D waterDepth;
layout(set = 3, binding = 0) uniform texture2D blurredGlassDepth;

#define WATER_DEPTH_OFFSET 0.15
#include "../Water/WaterTransparent.glh"

void main()
{
	if (CheckWaterDiscard())
	{
		color_out = vec4(0);
		return;
	}

	if (gl_FragCoord.z < texelFetch(blurredGlassDepth, ivec2(min(gl_FragCoord.xy, textureSize(blurredGlassDepth, 0) - 1)), 0).r)
		discard;

	vec2 scaledTC = texCoord_in * vec2(pc.tangent.w, pc.bitangent.w);
	vec2 waterDistSamplePos = vec2(texCoord_in.x, EG_FLIPGL(texCoord_in.y));
	float edgeDist = calcEdgeDist(scaledTC);
	float negScale = getInteractablesNegScale(
		worldPos_in, scaledTC, edgeDist, textureLod(sampler2D(waterDistanceTex, waterDistanceSampler), waterDistSamplePos, 0).r
	);

	color_out = calculateColor(scaledTC, edgeDist, negScale, 0);
}
