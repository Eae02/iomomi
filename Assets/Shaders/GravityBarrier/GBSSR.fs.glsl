#version 450 core

#include <Deferred.glh>

#define GB_WITH_BARRIER_SETTINGS_UB
#include "GBCommon.glh"

#include "../Inc/GBData.glh"
#include "../Inc/Depth.glh"
#include "../Inc/Light.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 screenCoord_in;

layout(location=0) out vec4 color_out;

layout(set=1, binding=0) uniform sampler2D gbColor2Sampler;
layout(set=1, binding=1) uniform sampler2D gbDepthSampler;

layout(set=2, binding=1) uniform sampler2D waterDistanceTex;

layout(constant_id=0) const float ssrMaxDistance = 20;

void main()
{
	float hDepth = texture(gbDepthSampler, screenCoord_in).r;
	vec4 gbc2 = texture(gbColor2Sampler, screenCoord_in);
	
	vec3 worldPos = WorldPosFromDepth(hDepth, screenCoord_in, renderSettings.invViewProjection);
	vec3 normal = SMDecode(gbc2.xy);
	
	vec3 toEye = normalize(renderSettings.cameraPosition - worldPos);
	vec3 rayDir = normalize(reflect(-toEye, normal));
	
	vec3 barrierNormal = normalize(cross(pc.tangent.xyz, pc.bitangent.xyz));
	float normalDotRayDir = dot(rayDir, barrierNormal);
	if (abs(normalDotRayDir) < 1E-6)
		discard;
	
	float intersectDist = dot(barrierNormal, pc.position - worldPos) / normalDotRayDir;
	gl_FragDepth = intersectDist / ssrMaxDistance;
	if (intersectDist < 0)
		discard;
	
	vec3 intersectPos = worldPos + rayDir * intersectDist;
	vec3 relativePos = intersectPos - pc.position;
	vec2 barrierSize = vec2(pc.tangent.w, pc.bitangent.w);
	vec2 barrierRad = barrierSize / 2.0;
	vec2 scaledTCCentered = vec2(dot(relativePos, pc.tangent.xyz), dot(relativePos, pc.bitangent.xyz));
	if (abs(scaledTCCentered.x) > barrierRad.x || abs(scaledTCCentered.y) > barrierRad.y)
		discard;
	vec2 scaledTC = scaledTCCentered + barrierRad;
	
	float waterDist = texture(waterDistanceTex, scaledTC / barrierSize).r;
	float edgeDist = calcEdgeDist(scaledTC);
	float negScale = getInteractablesNegScale(intersectPos, scaledTC, edgeDist, waterDist);
	vec4 barrierColor = calculateColor(scaledTC, edgeDist, negScale, 0);
	if (barrierColor.a < 0.95)
		discard;
	
	color_out = vec4(barrierColor.rgb, gl_FragDepth);
}
