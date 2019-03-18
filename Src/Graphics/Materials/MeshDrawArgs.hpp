#pragma once

#include "../Lighting/PointLightShadowMapper.hpp"

enum class MeshDrawMode
{
	Game,
	Editor,
	PointLightShadow
};

struct MeshDrawArgs
{
	MeshDrawMode drawMode;
	const PointLightShadowRenderArgs* plShadowRenderArgs;
};
