#version 450 core

layout(location=0) in flat uint mode_in;
layout(location=1) in vec2 pos2_in;
layout(location=2) in vec2 size_in;

#define MODE_BOX 0
#define MODE_PARTICLE 1

layout(location=0) out vec4 color_out;

const float RAD = 0.1;
const vec3 COLOR = vec3(0.12, 0.9, 0.7) * 2.0;

const float MIN_INTENSITY = 0.2;
const float INTENSITY_SCALE = 1.0;

layout(binding=1) uniform sampler2D particleSampler;

void main()
{
	if (mode_in == MODE_BOX)
	{
		vec2 fromEdge = size_in - abs(pos2_in * size_in);
		float a = pow(1.0 - clamp(min(fromEdge.x, fromEdge.y) / RAD, 0, 1) * (1.0 - MIN_INTENSITY), 2.0);
		color_out = vec4(COLOR * a * INTENSITY_SCALE, a);
	}
	else if (mode_in == MODE_PARTICLE)
	{
		color_out = vec4(texture(particleSampler, pos2_in * 0.5 + 0.5).r * COLOR, 0);
	}
}
