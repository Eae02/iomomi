#version 450 core

layout(location=0) in vec3 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec3 normal_in;
layout(location=3) in vec3 tangent_in;

layout(location=0) out vec3 worldPos_out;
layout(location=1) out vec2 texCoord_out;
layout(location=2) out vec3 normal_out;
layout(location=3) out vec3 tangent_out;

layout(binding=1) uniform GravityGunParamsUB
{
	mat4 viewProjTransform;
	float glowIntensityBoost;
};

layout(set=1, binding=0) uniform MeshTransformUB_UseDynamicOffset
{
	mat4 meshTransform;
};

void main()
{
	worldPos_out = (meshTransform * vec4(position_in, 1.0)).xyz;
	texCoord_out = texCoord_in;
	normal_out = normalize((meshTransform * vec4(normal_in, 0.0)).xyz);
	tangent_out = normalize((meshTransform * vec4(tangent_in, 0.0)).xyz);
	
	gl_Position = viewProjTransform * vec4(worldPos_out, 1.0);
}
