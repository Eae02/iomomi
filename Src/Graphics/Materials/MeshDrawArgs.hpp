#pragma once

#include "../Lighting/PointLightShadowMapper.hpp"
#include "../DeferredRenderer.hpp"

enum class MeshDrawMode
{
	Game,
	ObjectFlags,
	TransparentBeforeWater,
	TransparentAfterWater,
	Emissive,
	Editor,
	PointLightShadow,
	SpotLightShadow
};

struct MeshDrawArgs
{
	MeshDrawMode drawMode;
	const PointLightShadowRenderArgs* plShadowRenderArgs;
	eg::TextureRef waterDepthTexture;
};

enum ObjectRenderFlags : uint32_t
{
	Unlit = 0x1,
	NoSSR = 0x2
};
