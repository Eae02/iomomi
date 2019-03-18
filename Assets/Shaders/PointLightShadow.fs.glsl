#version 450 core

layout(location=0) in vec3 position_in;

layout(set=0, binding=0) uniform MatricesUB
{
	mat4 lightMatrices[6];
	vec4 lightSphere;
};

void main()
{
	gl_FragDepth = distance(position_in, lightSphere.xyz) / lightSphere.w;
}
