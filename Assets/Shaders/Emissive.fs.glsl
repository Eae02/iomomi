#version 450 core

//#pragma variants VBefWater VAftWater VNoWater

layout(location=0) out vec4 color_out;

layout(location=0) in vec4 color_in;

#ifndef VNoWater
layout(binding=1) uniform sampler2D waterDepth;
#endif

void main()
{
	color_out = color_in;
	
#ifdef VAftWater
	if (texture(waterDepth, ))
#endif
}
