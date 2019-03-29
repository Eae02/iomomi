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

layout(location=0) in vec4 worldPosAndClipDist_in[];
layout(location=0) out vec3 worldPos_out;

void main()
{
	int i = 0;
	while (i < 3)
	{
		gl_Layer = gl_InvocationID;
		gl_Position = lightMatrices[gl_InvocationID] * vec4(worldPosAndClipDist_in[i].xyz, 1.0);
		gl_ClipDistance[0] = worldPosAndClipDist_in[i].w;
		if (EG_VULKAN)
		{
			gl_Position.y = -gl_Position.y;
		}
		worldPos_out = worldPosAndClipDist_in[i].xyz;
		EmitVertex();
		i++;
	}
	EndPrimitive();
}
