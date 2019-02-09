#version 450 core

layout(location=0) out vec4 color_out;

layout(location=0) in float dash_in;

const float DASH_LEN = 0.1;

void main()
{
	if (mod(dash_in, DASH_LEN * 2) > DASH_LEN)
		discard;
	color_out = vec4(1.0);
}
