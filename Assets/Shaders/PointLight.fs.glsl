#include "Inc/Light.glh"
#include "Inc/DeferredLight.glh"

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
} pc;

layout(location=0) out vec4 color_out;

layout(set=0, binding=4) uniform samplerCube shadowMap;

vec3 sampleOffsetDirections[20] = vec3[]
(
	vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
	vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
	vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
	vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
	vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float sampleShadowMap(vec3 sampleVec)
{
	return textureLod(shadowMap, sampleVec, 0).r * pc.range + 0.01;
}

float getShadowFactor(vec3 worldPosition)
{
	vec3 lightIn = worldPosition - pc.position;
	float fragDepth = length(lightIn);
	lightIn /= fragDepth;
	
	float shadow = 0.0;
	
	float occluderDist = fragDepth - sampleShadowMap(lightIn);
	float diskRadius = 0.005 * (clamp(occluderDist * 2, 0.0, 3.0) + 1);
	
	for (int i = 0; i < sampleOffsetDirections.length(); i++)
	{
		vec3 offset = sampleOffsetDirections[i] * diskRadius;
		shadow += step(fragDepth, sampleShadowMap(lightIn + offset));
	}
	
	return shadow / float(sampleOffsetDirections.length());
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
