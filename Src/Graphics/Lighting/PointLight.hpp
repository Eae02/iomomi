#pragma once

#include "LightSource.hpp"

struct PointLightShadowDrawArgs
{
	glm::mat4 lightMatrix;
	eg::Sphere lightSphere;
	eg::Frustum frustum;
	bool renderStatic;
	bool renderDynamic;
	const class PointLight* light;
	
	void SetPushConstants() const;
};

class PointLight : public LightSource
{
public:
	PointLight()
		: LightSource(eg::ColorSRGB(1.0f, 1.0f, 1.0f), 20.0f) { }
	
	PointLight(const glm::vec3& pos, const eg::ColorSRGB& color, float intensity)
		: LightSource(color, intensity), position(pos) { }
	
	eg::Sphere GetSphere() const
	{
		return eg::Sphere(position, Range());
	}
	
	bool enabled = true;
	bool castsShadows = true;
	bool enableSpecularHighlights = true;
	bool willMoveEveryFrame = false;
	bool highPriority = false;
	glm::vec3 position;
	
	eg::TextureViewHandle shadowMap = nullptr;
};
