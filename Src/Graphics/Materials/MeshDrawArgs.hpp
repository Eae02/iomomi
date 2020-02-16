#pragma once

#include "../Lighting/PointLightShadowMapper.hpp"
#include "../DeferredRenderer.hpp"

enum class MeshDrawMode
{
	Game,
	Emissive,
	Transparent,
	Editor,
	PlanarReflection,
	PointLightShadow,
	SpotLightShadow
};

struct MeshDrawArgs
{
	MeshDrawMode drawMode;
	eg::Plane reflectionPlane;
	const DeferredRenderer::RenderTarget* renderTarget;
	const PointLightShadowRenderArgs* plShadowRenderArgs;
};
