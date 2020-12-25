#pragma once

#include "../Lighting/PointLightShadowMapper.hpp"
#include "../DeferredRenderer.hpp"

enum class MeshDrawMode
{
	Game,
	ObjectFlags,
	BlurredGlassDepthOnly,
	TransparentBeforeWater,
	TransparentBeforeBlur,
	TransparentFinal,
	Emissive,
	Editor,
	PointLightShadow,
	SpotLightShadow
};

struct MeshDrawArgs
{
	MeshDrawMode drawMode;
	const PointLightShadowDrawArgs* plShadowRenderArgs;
	eg::TextureRef waterDepthTexture;
	eg::TextureRef glassBlurTexture;
	RenderTexManager* rtManager;
};

enum ObjectRenderFlags : uint32_t
{
	Unlit = 0x1,
	NoSSR = 0x2
};
