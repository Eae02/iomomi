#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in float doorDist_in;

layout(location=0) out vec4 worldPosAndClipDist_out;

void main()
{
	worldPosAndClipDist_out = vec4(position_in, doorDist_in * 2.0 - 1.0);
}
