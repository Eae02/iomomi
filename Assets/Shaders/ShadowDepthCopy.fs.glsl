#version 450 core
#extension GL_EXT_samplerless_texture_functions : enable

layout(binding=0) uniform texture2D sourceShadowMap_UF;

void main()
{
	gl_FragDepth = texelFetch(sourceShadowMap_UF, ivec2(gl_FragCoord.xy), 0).r;
}
