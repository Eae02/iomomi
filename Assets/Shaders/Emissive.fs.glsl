#version 450 core

layout(location=0) out vec4 color_out;

layout(location=0) in vec4 color_in;

layout(constant_id=0) const float alpha = 1.0;

void main()
{
	color_out = vec4(color_in.rgb, alpha);
}
