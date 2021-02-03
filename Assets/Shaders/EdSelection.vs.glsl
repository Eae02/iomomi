#version 450 core

layout(location=0) in vec3 position_in;

layout(push_constant) uniform PC
{
	mat4 transform;
	float intensity;
};

void main()
{
	gl_Position = transform * vec4(position_in, 1.0);
}
