#version 450 core

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	vec3 color1;
	vec3 color2;
	float transitionProgress;
};

const float FADE_DIST = 0.5;
const float MIN_INTENSITY_SCALE = 0.5;

void main()
{
	float t = texCoord_in.y - transitionProgress;
	float i = (1.0 - abs(texCoord_in.x - 0.5) * 2 * (1 - MIN_INTENSITY_SCALE));
	color_out = vec4(i * mix(color1, color2, clamp((t / FADE_DIST) * 0.5 + 0.5, 0.0, 1.0)), 0.0);
}
