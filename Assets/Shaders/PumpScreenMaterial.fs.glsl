#version 450 core

layout(location=1) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(set=0, binding=1) uniform DataUB
{
	vec4 color;
	vec4 backgroundColor;
	vec4 arrowRects[4];
	vec4 arrowOpacities;
} data;

layout(set=0, binding=2) uniform sampler2D arrowTexture;

const float ARROW_SDF_EDGE_LO = 0.05;
const float ARROW_SDF_EDGE_HI = 0.10;

void main()
{
	float arrowIntensity = 0;
	for (int i = 0; i < 4; i++)
	{
		vec2 tc = (texCoord_in.yx - data.arrowRects[i].xy) * data.arrowRects[i].zw;
		float sdfVal = texture(arrowTexture, tc).r * step(abs(tc.y - 0.5), 0.5) * step(abs(tc.x - 0.5), 0.5);
		float ti = clamp((sdfVal - ARROW_SDF_EDGE_LO) / (ARROW_SDF_EDGE_HI - ARROW_SDF_EDGE_LO), 0, 1);
		arrowIntensity = max(arrowIntensity, ti * data.arrowOpacities[i]);
	}
	color_out = mix(data.backgroundColor, data.color, arrowIntensity);
}
