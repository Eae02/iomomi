#version 450 core

layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

const int NUM_ARROWS = 8;

layout(set=0, binding=1) uniform DataUB
{
	vec4 color;
	vec4 arrowRects[NUM_ARROWS];
	vec4 arrowOpacities[NUM_ARROWS / 4];
} data;

layout(set=0, binding=2) uniform sampler2D arrowTexture;

const float ARROW_SDF_EDGE_LO = 0.05;
const float ARROW_SDF_EDGE_HI = 0.10;

const float SCANLINE_DIST_MAX = 0.015;
const float SCANLINE_DIST_FALLOFF = 2;

void main()
{
	float arrowIntensity = 0;
	for (int i = 0; i < NUM_ARROWS; i++)
	{
		vec2 tc = (texCoord_in.yx - data.arrowRects[i].xy) * data.arrowRects[i].zw;
		float ti = clamp((texture(arrowTexture, tc).r * step(abs(tc.y - 0.5), 0.5) - ARROW_SDF_EDGE_LO) / (ARROW_SDF_EDGE_HI - ARROW_SDF_EDGE_LO), 0, 1);
		arrowIntensity = max(arrowIntensity, ti * data.arrowOpacities[i / 4][i % 4]);
	}
	color_out = data.color * mix(0.5, 5.0, arrowIntensity);
}
