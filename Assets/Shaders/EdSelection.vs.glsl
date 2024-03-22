#version 450 core

layout(location=0) in vec3 position_in;

layout(set=0, binding=0) uniform Params_UseDynamicOffset
{
	mat4 transform;
	float intensity;
};

void main()
{
	gl_Position = transform * vec4(position_in, 1.0);
}
