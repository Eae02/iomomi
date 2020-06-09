#pragma once

#include "../Lighting/PointLightShadowMapper.hpp"
#include "../DeferredRenderer.hpp"

enum class MeshDrawMode
{
	Game,
	ObjectFlags,
	Emissive,
	Transparent,
	Editor,
	PointLightShadow,
	SpotLightShadow
};

struct MeshDrawArgs
{
	MeshDrawMode drawMode;
	const DeferredRenderer::RenderTarget* renderTarget;
	const PointLightShadowRenderArgs* plShadowRenderArgs;
};

enum ObjectRenderFlags : uint32_t
{
	Unlit = 0x1,
	NoSSR = 0x2
};
