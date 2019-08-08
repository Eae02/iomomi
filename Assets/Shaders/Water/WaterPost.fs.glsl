#version 450 core

#include <EGame.glh>
#include <Deferred.glh>

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D waterDepthSampler;
layout(binding=2) uniform sampler2D worldColorSampler;
layout(binding=3) uniform sampler2D worldDepthSampler;

layout(binding=4) buffer FragmentsUnderwaterBuffer
{
	uint numFragmentsUnderwater;
};

const float C1 = 0.429043;
const float C2 = 0.511664;
const float C3 = 0.743125;
const float C4 = 0.886227;
const float C5 = 0.247708;

const vec3 L00  = vec3( 0.7870665,  0.9379944,  0.9799986);
const vec3 L1m1 = vec3( 0.4376419,  0.5579443,  0.7024107);
const vec3 L10  = vec3(-0.1020717, -0.1824865, -0.2749662);
const vec3 L11  = vec3( 0.4543814,  0.3750162,  0.1968642);
const vec3 L2m2 = vec3( 0.1841687,  0.1396696,  0.0491580);
const vec3 L2m1 = vec3(-0.1417495, -0.2186370, -0.3132702);
const vec3 L20  = vec3(-0.3890121, -0.4033574, -0.3639718);
const vec3 L21  = vec3( 0.0872238,  0.0744587,  0.0353051);
const vec3 L22  = vec3( 0.6662600,  0.6706794,  0.5246173);

const float R0 = 0.03;
const float roughness = 0.2;

const vec3 color = pow(vec3(13, 184, 250) / 255.0, vec3(2.2));

const vec3 colorSurface = vec3(0.04, 0.59, 0.77);
const vec3 colorDeep = vec3(0.01, 0.03, 0.35);
const float visibility = 20;
const vec3 colorExtinction = vec3(0.7, 3.0, 4.0) * 10;

const float indexOfRefraction = 0.6;
const float reflectDistortionFactor = 0.05;

const vec3 waterUp = vec3(0, 1, 0);

const float causticsPosScale = 20;
const float causticsTimeScale = 0.05;
const float causticsIntensity = 0.04;
const float causticsPower = 5.0;

vec3 doColorExtinction(vec3 inputColor, float waterTravelDist)
{
	vec3 step1Res = mix(inputColor, colorSurface, clamp(waterTravelDist / visibility, 0.0, 1.0));
	return mix(step1Res, colorDeep, clamp(min(waterTravelDist, 10) / (colorExtinction), vec3(0.0), vec3(1.0)));
}

float fresnelSchlick(float cosT, float F0, float roughness)
{
	return F0 + (max(1.0 - roughness, F0) - F0) * pow(1.0 - cosT, 5.0);
}

vec3 sh(vec3 dir)
{
	return C1 * L22 * (dir.x * dir.x - dir.y * dir.y) +
	                  C3 * L20 * dir.z * dir.z +
	                  C4 * L00 -
	                  C5 * L20 +
	                  2.0 * C1 * L2m2 * dir.x * dir.y +
	                  2.0 * C1 * L21  * dir.x * dir.z +
	                  2.0 * C1 * L2m1 * dir.y * dir.z +
	                  2.0 * C2 * L11  * dir.x +
	                  2.0 * C2 * L1m1 * dir.y +
	                  2.0 * C2 * L10  * dir.z;
}

const float ZNear = 0.1;
const float ZFar = 1000.0;
float hyperDepth(float lDepth)
{
	return (((2 * ZNear * ZFar) / lDepth) - ZFar - ZNear) / (ZNear - ZFar);
}

float linearizeDepth(float hDepth)
{
	return 2.0 * ZNear * ZFar / (ZFar + ZNear - (hDepth * (ZFar - ZNear)));
}

vec3 getEyePosDepth(vec2 screenCoord, float depthH)
{
	vec4 h = vec4(screenCoord * 2.0 - vec2(1.0), EG_OPENGL ? (depthH * 2.0 - 1.0) : depthH, 1.0);
	if (!EG_OPENGL)
		h.y = -h.y;
	vec4 d = renderSettings.invProjectionMatrix * h;
	return d.xyz / d.w;
}

vec3 getEyePos(vec2 screenCoord)
{
	return getEyePosDepth(screenCoord, hyperDepth(texture(waterDepthSampler, screenCoord).r));
}

void main()
{
	vec2 pixelSize = 1.0 / vec2(textureSize(waterDepthSampler, 0));
	
	vec2 depthAndTravelDist = texture(waterDepthSampler, texCoord_in).rg;
	float depthL = depthAndTravelDist.r;
	
	bool underwater = depthL < 1.5;
	
	vec3 worldColor = texture(worldColorSampler, texCoord_in).rgb;
	
	float depthH = hyperDepth(depthL);
	float worldDepthH = texture(worldDepthSampler, texCoord_in).r;
	if (worldDepthH < depthH)
	{
		color_out = vec4(worldColor, 1.0);
		return;
	}
	
	vec3 posEye = getEyePosDepth(texCoord_in, depthH);
	
	vec3 ddx = getEyePos(texCoord_in + vec2(pixelSize.x, 0)) - posEye;
	vec3 ddx2 = posEye - getEyePos(texCoord_in - vec2(pixelSize.x, 0));
	if (abs(ddx.z) > abs(ddx2.z))
	{
		ddx = ddx2;
	}
	
	vec3 ddy = getEyePos(texCoord_in + vec2(0, pixelSize.y)) - posEye;
	vec3 ddy2 = posEye - getEyePos(texCoord_in - vec2(0, pixelSize.y));
	if (abs(ddy2.z) < abs(ddy.z))
	{
		ddy = ddy2;
	}
	
	vec3 normal = normalize((renderSettings.invViewMatrix * vec4(cross(ddy, ddx), 0.0)).xyz);
	vec3 surfacePos = WorldPosFromDepth(depthH, texCoord_in, renderSettings.invViewProjection);
	vec3 targetPos = WorldPosFromDepth(worldDepthH, texCoord_in, renderSettings.invViewProjection);
	
	float waterTravelDist = min(depthAndTravelDist.g * 0.25, distance(targetPos, surfacePos));
	
	vec3 cameraPos = renderSettings.invViewMatrix[3].xyz;
	
	vec3 dirToEye = normalize(cameraPos - surfacePos);
	
	//vec3 distortMoveVec = waterUp * (1.0 - dot(normal, waterUp));
	
	//The vector to move by to get to the reflection coordinate
	//vec3 reflectMoveVec = distortMoveVec * reflectDistortionFactor;
	
	//Finds the reflection coordinate in screen space
	//vec4 screenSpaceReflectCoord = renderSettings.viewProj * vec4(position_in + reflectMoveVec, 1.0);
	//screenSpaceReflectCoord.xyz /= screenSpaceReflectCoord.w;
	
	//vec3 reflectColor = texture(reflectionMap, (screenSpaceReflectCoord.xy + vec2(1.0)) / 2.0).xyz;
	vec3 reflectColor = sh(reflect(dirToEye, normal));
	
	vec3 refractColor = doColorExtinction(worldColor, waterTravelDist);
	
	if (underwater)
	{
		color_out = vec4(refractColor, 1.0);
		atomicAdd(numFragmentsUnderwater, 1);
	}
	else
	{
		const float MIN_FADE_DEPTH = 0.0;
		const float MAX_FADE_DEPTH = 0.0001;
		float shoreSmooth = clamp((worldDepthH - depthH) / MAX_FADE_DEPTH, 0.0, 1.0);
		
		float fresnel = fresnelSchlick(max(dot(normal, dirToEye), 0.0), R0, roughness);
		
		vec3 lightDir   = normalize(vec3(0, 0, 0) - surfacePos);
		vec3 viewDir    = normalize(cameraPos - surfacePos);
		vec3 halfwayDir = normalize(lightDir + viewDir);
		
		const vec3 lightColor = vec3(1, 0.8, 0.8);
		
		const float shininess = 5;
		float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
		vec3 light = lightColor * spec + 0.5;
		
		color_out = vec4(mix(worldColor, mix(refractColor, reflectColor, fresnel) * light, shoreSmooth), 1.0);
	}
}
