#version 450 core

#include <EGame.glh>

layout(triangles) in;
layout(invocations=6) in;

layout(triangle_strip) out;
layout(max_vertices=3) out;

layout(set=0, binding=0) uniform MatricesUB
{
	mat4 lightMatrices[6];
	vec4 lightSphere;
};

layout(location=0) in vec3 worldPos_in[];
layout(location=0) out vec3 worldPos_out;

layout(location=1) in vec2 texCoord_in[];
layout(location=1) out vec2 texCoord_out;

void main()
{
	int i = 0;
	while (i < 3)
	{
		gl_Layer = gl_InvocationID;
		gl_Position = lightMatrices[gl_InvocationID] * vec4(worldPos_in[i], 1.0);
		if (EG_VULKAN)
		{
			gl_Position.y = -gl_Position.y;
		}
		worldPos_out = worldPos_in[i];
		texCoord_out = texCoord_in[i];
		EmitVertex();
		i++;
	}
	EndPrimitive();
}
