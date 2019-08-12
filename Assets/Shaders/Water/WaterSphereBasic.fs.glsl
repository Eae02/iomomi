#version 450 core

#include "WaterCommon.glh"

#define RENDER_SETTINGS_BINDING 0
#include "../Inc/RenderSettings.glh"

layout(location=0) in vec2 spritePos_in;
layout(location=1) in vec3 eyePos_in;

layout(location=0) out vec4 color_out;

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

const vec3 color = pow(vec3(13, 184, 250) / 255.0, vec3(2.2));

void main()
{
	float r2 = dot(spritePos_in, spritePos_in);
	if (r2 > 1.0)
		discard;
	vec3 normalES = vec3(spritePos_in, sqrt(1.0 - r2));
	vec4 ppsPos = renderSettings.projectionMatrix * vec4(eyePos_in + normalES * PARTICLE_RADIUS, 1.0);
	gl_FragDepth = ppsPos.z / ppsPos.w;
	
	vec3 normal = normalize((renderSettings.invViewMatrix * vec4(normalES, 0)).xyz);
	
	vec3 irradiance = C1 * L22 * (normal.x * normal.x - normal.y * normal.y) +
	                  C3 * L20 * normal.z * normal.z +
	                  C4 * L00 -
	                  C5 * L20 +
	                  2.0 * C1 * L2m2 * normal.x * normal.y +
	                  2.0 * C1 * L21  * normal.x * normal.z +
	                  2.0 * C1 * L2m1 * normal.y * normal.z +
	                  2.0 * C2 * L11  * normal.x +
	                  2.0 * C2 * L1m1 * normal.y +
	                  2.0 * C2 * L10  * normal.z;
	irradiance = mix(irradiance, vec3(1.0), 0.5);
	
	color_out = vec4(color * irradiance, 1.0);
}
