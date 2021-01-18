#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec4 color_in;

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	mat4 viewProj;
};

void main()
{
	color_out = vec4(pow(color_in.rgb, vec3(2.2)), color_in.a);
	gl_Position = viewProj * vec4(position_in, 1.0);
	gl_Position.z -= 0.001;
}
