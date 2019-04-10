#pragma once

#include "../Lighting/PointLightShadowMapper.hpp"

enum class MeshDrawMode
{
	Game,
	Emissive,
	Editor,
	PlanarReflection,
	PointLightShadow
};

struct MeshDrawArgs
{
	MeshDrawMode drawMode;
	eg::Plane reflectionPlane;
	const PointLightShadowRenderArgs* plShadowRenderArgs;
};
