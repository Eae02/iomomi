#version 450 core

#include <EGame.glh>

const vec2 positions[] = vec2[]
(
	vec2(-1, -1),
	vec2(-1,  3),
	vec2( 3, -1)
);

layout(location=0) out vec2 texCoord_out;

void main()
{
	gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
	texCoord_out = gl_Position.xy * vec2(0.5, -0.5) + vec2(0.5);
	texCoord_out.y = EG_FLIPGL(texCoord_out.y);
}
