#version 450 core

#pragma variants VLowQual VStdQual VHighQual

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

#ifndef VLowQual
layout(binding=4) uniform sampler2D normalMapSampler;
#endif

const float R0 = 0.03;

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

vec3 viewPosFromDepth(vec2 screenCoord, float depthH)
{
	vec4 h = vec4(screenCoord * 2.0 - vec2(1.0), EG_OPENGL ? (depthH * 2.0 - 1.0) : depthH, 1.0);
	if (!EG_OPENGL)
		h.y = -h.y;
	vec4 d = renderSettings.invProjectionMatrix * h;
	return d.xyz / d.w;
}

vec3 viewPosSampleDepth(vec2 screenCoord)
{
	return viewPosFromDepth(screenCoord, hyperDepth(texture(waterDepthSampler, screenCoord).r));
}

vec3 calcReflection(vec3 surfacePos, vec3 dirToEye, vec3 normal)
{
	const vec3 DEFAULT_REFLECT_COLOR = vec3(0.5, 0.6, 0.7);
	
#ifdef VHighQual
	// ** Screen space reflections **
	
	vec3 rayDir = normalize(reflect(-dirToEye, normal));
	
	const float MAX_DIST   = 15;   //Maximum distance to reflection
	const float FADE_BEGIN = 0.75; //Percentage of screen radius to begin fading out at
	const int   SAMPLES    = 32;   //Number of samples to take along the ray
	
	for (uint i = 1; i <= SAMPLES; i++)
	{
		vec3 sampleWorldPos = surfacePos + rayDir * (i * (MAX_DIST / SAMPLES));
		vec4 sampleNDC4 = renderSettings.viewProjection * vec4(sampleWorldPos, 1);
		vec3 sampleNDC = sampleNDC4.xyz / sampleNDC4.w;
		if (sampleNDC.x < -1 || sampleNDC.y < -1 || sampleNDC.x > 1 || sampleNDC.y > 1)
			break;
		
		vec2 sampleTC = sampleNDC.xy * 0.5 + 0.5;
		if (!EG_OPENGL)
			sampleTC.y = 1 - sampleTC.y;
		
		if (depthTo01(sampleNDC.z) > texture(worldDepthSampler, sampleTC).r)
		{
			float fade01 = max(abs(sampleNDC.x), abs(sampleNDC.y));
			float fade = clamp((fade01 - 1) / (1 - FADE_BEGIN) + 1, 0, 1);
			return mix(texture(worldColorSampler, sampleTC).rgb, DEFAULT_REFLECT_COLOR, fade);
		}
	}
#endif
	
	return DEFAULT_REFLECT_COLOR;
}

const vec2 normalMapPanDirs[] = vec2[]
(
	vec2(1, 1),
	vec2(0.5, -0.5),
	vec2(-1.5, 1.5),
	vec2(-1.0, -1.0)
);

layout(push_constant) uniform PC
{
	vec2 pixelSize;
};

void main()
{
	vec2 depthAndTravelDist = texture(waterDepthSampler, texCoord_in).rg;
	
	bool underwater = depthAndTravelDist.r < 2.0;
	
	vec3 worldColor = texture(worldColorSampler, texCoord_in).rgb;
	
	float depthH = hyperDepth(depthAndTravelDist.r);
	float worldDepthH = texture(worldDepthSampler, texCoord_in).r;
	
	//If the world is closer than the water, don't render water
	if (worldDepthH < depthH)
	{
		color_out = vec4(worldColor, 1.0);
		return;
	}
	
	//Stores values without refraction applied for fading later
	vec3 oriWorldColor = worldColor;
	float worldDepthL = linearizeDepth(worldDepthH);
	float fadeDepth = worldDepthL - depthAndTravelDist.r;
	
	vec3 surfacePos = WorldPosFromDepth(depthH, texCoord_in, renderSettings.invViewProjection);
	
	vec3 posEye = viewPosFromDepth(texCoord_in, depthH);
	
	//Finds the difference in view space position along the x-axis
	vec3 ddx = viewPosSampleDepth(texCoord_in + vec2(pixelSize.x, 0)) - posEye;
	vec3 ddx2 = posEye - viewPosSampleDepth(texCoord_in - vec2(pixelSize.x, 0));
	if (abs(ddx.z) > abs(ddx2.z))
	{
		ddx = ddx2;
	}
	
	//Finds the difference in view space position along the y-axis
	float offsetY = EG_OPENGL ? -pixelSize.y : pixelSize.y;
	vec3 ddy = viewPosSampleDepth(texCoord_in + vec2(0, offsetY)) - posEye;
	vec3 ddy2 = posEye - viewPosSampleDepth(texCoord_in - vec2(0, offsetY));
	if (abs(ddy2.z) < abs(ddy.z))
	{
		ddy = ddy2;
	}
	
	//Uses the view space differences to approximate the normal
	vec3 plainNormal = normalize((renderSettings.invViewMatrix * vec4(cross(ddy, ddx), 0.0)).xyz);
	
#ifdef VLowQual
	vec3 normal = plainNormal;
#else
	vec3 plainTangent = normalize((renderSettings.invViewMatrix * vec4(ddy, 0.0)).xyz);
	mat3 tbnMatrix = mat3(plainTangent, cross(plainTangent, plainNormal), plainNormal);
	
	//Normal mapping using triplanar mapping
	vec3 normalTS = vec3(0, 0, 0);
	for (int d = 0; d < 3; d++)
	{
		vec2 worldPos2 = vec2(surfacePos[(d + 1) % 3], surfacePos[(d + 2) % 3]);
		for (int i = 0; i < normalMapPanDirs.length(); i++)
		{
			vec2 nmMoveOrtho = vec2(normalMapPanDirs[i].y, -normalMapPanDirs[i].x);
			vec2 samplePos = vec2(dot(worldPos2, normalMapPanDirs[i]), dot(worldPos2, nmMoveOrtho) + renderSettings.gameTime * 0.2);
			vec2 nmVal = texture(normalMapSampler, samplePos * 0.1).xy * (255.0 / 128.0) - 1.0;
			normalTS += vec3(nmVal, sqrt(1 - (nmVal.x * nmVal.x + nmVal.y * nmVal.y))) * abs(plainNormal[d]);
		}
	}
	const float nmStrength = 0.3;
	normalTS.xy *= nmStrength;
	vec3 normal = normalize(tbnMatrix * normalTS);
#endif
	
	vec3 dirToEye = normalize(renderSettings.cameraPosition - surfacePos);
	
	vec3 targetPos = WorldPosFromDepth(worldDepthH, texCoord_in, renderSettings.invViewProjection);
	
#ifndef VLowQual
	//Refraction
	if (!underwater)
	{
		//The refracted texture coordinate is found by moving slightly
		// along the refraction vector in world space and reprojecting.
		
		vec3 refraction = refract(-dirToEye, underwater ? -normal : normal, indexOfRefraction);
		vec3 refractMoveVec = refraction * min((worldDepthL - depthAndTravelDist.r) * 0.2, 2.0);
		vec4 screenSpaceRefractCoord = renderSettings.viewProjection * vec4(surfacePos + refractMoveVec, 1.0);
		screenSpaceRefractCoord.xyz /= screenSpaceRefractCoord.w;
		
		vec2 refractTexcoordTarget = (screenSpaceRefractCoord.xy + vec2(1.0)) / 2.0;
		if (EG_VULKAN)
			refractTexcoordTarget.y = 1 - refractTexcoordTarget.y;
		refractTexcoordTarget = clamp(refractTexcoordTarget, vec2(0.0), vec2(1.0));
		
		//Steps between the refracted and not refracted texture coordinates
		// to minimize the jump from pixels where the refracted texture coordinate
		// is valid (not above water) and those where it is not.
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
		
		//Updates variables with the new texture coordinate
		worldColor = texture(worldColorSampler, refractTexcoord).rgb;
		targetPos = WorldPosFromDepth(worldDepthH, refractTexcoord, renderSettings.invViewProjection);
		depthAndTravelDist = texture(waterDepthSampler, refractTexcoord).rg;
	}
#endif
	
	float waterTravelDist = min(depthAndTravelDist.g * 0.5, distance(targetPos, surfacePos));
	
	vec3 reflectColor = calcReflection(surfacePos, dirToEye, normal);
	
	vec3 refractColor = doColorExtinction(worldColor, waterTravelDist);
	
	if (underwater)
	{
		color_out = vec4(refractColor, 1.0);
	}
	else
	{
		const float FOAM_FADE_MAX = 0.7;
		const float FOAM_FADE_BEGIN = 1.5;
		float foam = clamp((fadeDepth - FOAM_FADE_MAX) / (FOAM_FADE_BEGIN - FOAM_FADE_MAX), 0.0, 1.0);
		
#ifdef VLowQual
		float shore = 1;
#else
		const float SHORE_FADE_MAX = 0.4;
		const float SHORE_FADE_BEGIN = 0.6;
		float shore = clamp((fadeDepth - SHORE_FADE_MAX) / (SHORE_FADE_BEGIN - SHORE_FADE_MAX), 0.0, 1.0);
#endif
		
		float fresnel = R0 + (1.0 - R0) * pow(1.0 - max(dot(normal, dirToEye), 0.0), 5.0);
		
		vec3 light = vec3(0.8, 0.9, 1.0) * abs(dot(normal, normalize(vec3(1, 1, 1)))) * 0.25 + 0.5;
		vec3 waterColor = mix(refractColor, reflectColor, fresnel) * light;
		
		vec3 foamColor = vec3(1.0);
		
		color_out = vec4(mix(oriWorldColor, mix(foamColor, waterColor, foam), shore), 1.0);
	}
}
