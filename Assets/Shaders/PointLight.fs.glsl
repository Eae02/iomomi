#include "Inc/Light.glh"
#include "Inc/DeferredLight.glh"

layout(push_constant, std140) uniform PC
{
	vec3 position;
	float range;
	vec3 radiance;
} pc;

layout(location=0) out vec4 color_out;

void main()
{
	vec2 screenCoord = gl_FragCoord.xy / vec2(textureSize(gbColor1Sampler, 0));
	GBData gbData = ReadGB(screenCoord);
	
	vec3 toLight = pc.position - gbData.worldPos;
	float dist = length(toLight);
	toLight /= dist;
	
	vec3 toEye = normalize(renderSettings.cameraPosition - gbData.worldPos);
	
	vec3 fresnel = calcFresnel(gbData, toEye);
	
	vec3 radiance = pc.radiance * calcAttenuation(dist);
	
	color_out = vec4(calcDirectReflectance(toLight, toEye, fresnel, gbData, radiance), 0.0);
}
