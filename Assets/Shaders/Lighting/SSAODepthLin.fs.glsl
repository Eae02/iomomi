#version 450 core

#include "../Inc/Depth.glh"

layout(binding=0) uniform sampler2D gbDepthSampler;

layout(location=0) in vec2 screenCoord_in;

layout(location=0) out float depth_out;

void main()
{
	depth_out = linearizeDepth(texture(gbDepthSampler, screenCoord_in).r);
}
