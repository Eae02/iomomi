#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=0) uniform sampler2D gbColor1Sampler;

layout(push_constant) uniform PC
{
	vec3 ambient;
};

void main()
{
	vec4 c1 = texture(gbColor1Sampler, texCoord_in);
	color_out = vec4(c1.xyz * (c1.w > 0.99 ? vec3(1.0) : (ambient * c1.w)), 1.0);
}
