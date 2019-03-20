#include "Inc/Light.glh"
#include "Inc/DeferredLight.glh"

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
} pc;

layout(location=0) out vec4 color_out;

layout(set=0, binding=4) uniform samplerCubeShadow shadowMap;

const vec3 SAMPLE_OFFSETS[] = vec3[]
(
	vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
	vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
	vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
	vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
	vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

const float SAMPLE_DIST = 0.005;
const float DEPTH_BIAS = 0.001;

float getShadowFactor(vec3 worldPosition)
{
	vec3 lightIn = worldPosition - pc.position;
	float fragDepth = length(lightIn);
	lightIn /= fragDepth;
	
	float shadow = 0.0;
	
	float compare = fragDepth / pc.range - DEPTH_BIAS;
	for (int i = 0; i < SAMPLE_OFFSETS.length(); i++)
	{
		vec3 samplePos = lightIn + SAMPLE_OFFSETS[i] * SAMPLE_DIST;
		shadow += texture(shadowMap, vec4(samplePos, compare)).r;
	}
	
	return shadow / float(SAMPLE_OFFSETS.length());
}

void main()
{
	vec2 screenCoord = gl_FragCoord.xy / vec2(textureSize(gbColor1Sampler, 0));
	GBData gbData = ReadGB(screenCoord);
	
	vec3 toLight = pc.position - gbData.worldPos;
	float dist = length(toLight);
	toLight /= dist;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	vec3 radiance = pc.radiance * calcAttenuation(dist) * getShadowFactor(gbData.worldPos);
	
	color_out = vec4(calcDirectReflectance(toLight, toEye, fresnel, gbData, radiance), 0.0);
}
