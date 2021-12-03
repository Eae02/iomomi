#version 450 core

#include "GBCommon.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

const float BASE_INTENSITY = 5;

void main()
{
	vec2 scaledTC = texCoord_in * vec2(pc.tangent.w, pc.bitangent.w);
	color_out = calculateColor(scaledTC, 0, BASE_INTENSITY);
}
