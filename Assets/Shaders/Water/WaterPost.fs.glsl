#version 450 core

#pragma variants VStdQual VLowQual

#include <EGame.glh>
#include <Deferred.glh>

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

#include "../Inc/Depth.glh"

layout(location=0) in vec2 texCoord_in;

layout(location=0) out vec4 color_out;

layout(binding=1) uniform sampler2D waterDepthSampler;
layout(binding=2) uniform sampler2D worldColorSampler;
layout(binding=3) uniform sampler2D worldDepthSampler;

#ifdef VStdQual
layout(binding=4) uniform sampler2D normalMapSampler;
#endif

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

const vec2 normalMapMoveDirs[] = vec2[] (
	vec2(1, 1),
	vec2(0.5, -0.5),
	vec2(-1.5, 1.5),
	vec2(-1.0, -1.0)
);

void main()
{
	vec2 pixelSize = 1.0 / vec2(textureSize(waterDepthSampler, 0));
	
	vec2 depthAndTravelDist = texture(waterDepthSampler, texCoord_in).rg;
	
	bool underwater = depthAndTravelDist.r < 2.0;
	
	vec3 worldColor = texture(worldColorSampler, texCoord_in).rgb;
	
	float depthH = hyperDepth(depthAndTravelDist.r);
	float worldDepthH = texture(worldDepthSampler, texCoord_in).r;
	if (worldDepthH < depthH)
	{
		color_out = vec4(worldColor, 1.0);
		return;
	}
	
	vec3 oriWorldColor = worldColor;
	float worldDepthL = linearizeDepth(worldDepthH);
	float fadeDepth = min(depthAndTravelDist.g * 0.25, worldDepthL - depthAndTravelDist.r);
	
	vec3 surfacePos = WorldPosFromDepth(depthH, texCoord_in, renderSettings.invViewProjection);
	
	vec3 posEye = getEyePosDepth(texCoord_in, depthH);
	
	vec3 ddx = getEyePos(texCoord_in + vec2(pixelSize.x, 0)) - posEye;
	vec3 ddx2 = posEye - getEyePos(texCoord_in - vec2(pixelSize.x, 0));
	if (abs(ddx.z) > abs(ddx2.z))
	{
		ddx = ddx2;
	}
	
	float offsetY = EG_OPENGL ? -pixelSize.y : pixelSize.y;
	vec3 ddy = getEyePos(texCoord_in + vec2(0, offsetY)) - posEye;
	vec3 ddy2 = posEye - getEyePos(texCoord_in - vec2(0, offsetY));
	if (abs(ddy2.z) < abs(ddy.z))
	{
		ddy = ddy2;
	}
	
	vec3 plainNormal = normalize((renderSettings.invViewMatrix * vec4(cross(ddy, ddx), 0.0)).xyz);
	
#ifdef VLowQual
	vec3 normal = plainNormal;
#else
	vec3 plainTangent = normalize((renderSettings.invViewMatrix * vec4(ddy, 0.0)).xyz);
	mat3 tbnMatrix = mat3(plainTangent, cross(plainTangent, plainNormal), plainNormal);
	
	vec3 normalTS = vec3(0, 0, 0);
	for (int d = 0; d < 3; d++)
	{
		vec2 worldPos2 = vec2(surfacePos[(d + 1) % 3], surfacePos[(d + 2) % 3]);
		for (int i = 0; i < normalMapMoveDirs.length(); i++)
		{
			vec2 nmMoveL = vec2(normalMapMoveDirs[i].y, -normalMapMoveDirs[i].x);
			vec2 samplePos = vec2(dot(worldPos2, normalMapMoveDirs[i]), dot(worldPos2, nmMoveL) + renderSettings.gameTime * 0.2);
			vec2 nmVal = texture(normalMapSampler, samplePos * 0.2).xy * (255.0 / 128.0) - 1.0;
			normalTS += vec3(nmVal, sqrt(1 - (nmVal.x * nmVal.x + nmVal.y * nmVal.y))) * abs(plainNormal[d]);
		}
	}
	const float nmStrength = 0.5;
	normalTS.xy *= nmStrength;
	vec3 normal = normalize(tbnMatrix * normalTS);
#endif
	
	vec3 cameraPos = renderSettings.invViewMatrix[3].xyz;
	
	vec3 dirToEye = normalize(cameraPos - surfacePos);
	
	vec3 targetPos = WorldPosFromDepth(worldDepthH, texCoord_in, renderSettings.invViewProjection);
	
#ifdef VStdQual
	//Refraction
	if (!underwater)
	{
		vec3 refraction = refract(-dirToEye, underwater ? -normal : normal, indexOfRefraction);
		vec3 refractMoveVec = refraction * min((worldDepthL - depthAndTravelDist.r) * 0.2, 1.0);
		vec3 refractPos = surfacePos + refractMoveVec;
		
		vec4 screenSpaceRefractCoord = renderSettings.viewProjection * vec4(refractPos, 1.0);
		screenSpaceRefractCoord.xyz /= screenSpaceRefractCoord.w;
		
		vec2 refractTexcoordTarget = (screenSpaceRefractCoord.xy + vec2(1.0)) / 2.0;
		if (EG_VULKAN)
			refractTexcoordTarget.y = 1 - refractTexcoordTarget.y;
		refractTexcoordTarget = clamp(refractTexcoordTarget, vec2(0.0), vec2(1.0));
		
		vec2 refractTexcoord;
		const int REFRACT_TRIES = 4;
		for (int i = 0; i <= REFRACT_TRIES; i++)
		{
			refractTexcoord = mix(refractTexcoordTarget, texCoord_in, i / float(REFRACT_TRIES));
			float refractedWorldDepthH = texture(worldDepthSampler, refractTexcoord).r;
			if (refractedWorldDepthH > depthH)
			{
				worldDepthH = refractedWorldDepthH;
				break;
			}
		}
		worldColor = texture(worldColorSampler, refractTexcoord).rgb;
		targetPos = WorldPosFromDepth(worldDepthH, refractTexcoord, renderSettings.invViewProjection);
		depthAndTravelDist = texture(waterDepthSampler, refractTexcoord).rg;
	}
#endif
	
	float waterTravelDist = min(depthAndTravelDist.g * 0.25, distance(targetPos, surfacePos));
	
	vec3 reflectColor = vec3(0.5, 0.7, 0.8);
	
	vec3 refractColor = doColorExtinction(worldColor, waterTravelDist);
	
	if (underwater)
	{
		color_out = vec4(refractColor, 1.0);
	}
	else
	{
		const float FOAM_FADE_1 = 0.3;
		const float FOAM_FADE_0 = 0.5;
		float foam = clamp((fadeDepth - FOAM_FADE_1) / (FOAM_FADE_0 - FOAM_FADE_1), 0.0, 1.0);
		
		const float SHORE_FADE_1 = 0.0;
		const float SHORE_FADE_0 = 0.2;
		float shore = clamp((fadeDepth - FOAM_FADE_1) / (FOAM_FADE_0 - FOAM_FADE_1), 0.0, 1.0);
		
		float fresnel = fresnelSchlick(max(dot(normal, dirToEye), 0.0), R0, roughness);
		
		vec3 light = vec3(0.8, 0.9, 1.0) * abs(dot(normal, normalize(vec3(1, 1, 1)))) * 0.25 + 0.5;
		vec3 waterColor = mix(refractColor, reflectColor, fresnel) * light;
		
		vec3 foamColor = vec3(1.0);
		
		color_out = vec4(mix(oriWorldColor, mix(foamColor, waterColor, foam), shore), 1.0);
	}
}
