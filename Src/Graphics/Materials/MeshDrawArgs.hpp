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
	const PointLightShadowRenderArgs* plShadowRenderArgs;
};
