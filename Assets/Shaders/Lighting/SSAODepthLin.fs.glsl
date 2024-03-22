#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

#include "../Inc/Depth.glh"

layout(binding=0) uniform texture2D gbDepthTex_UF;

layout(location=0) out float depth_out;

void main()
{
	depth_out = linearizeDepth(texelFetch(gbDepthTex_UF, ivec2(gl_FragCoord.xy), 0).r);
}
