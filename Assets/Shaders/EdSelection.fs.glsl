#version 450 core

layout(location=0) out vec4 color_out;

layout(set=0, binding=0) uniform Params_UseDynamicOffset
{
	mat4 transform;
	float intensity;
};

void main()
{
	color_out = vec4(intensity);
}
