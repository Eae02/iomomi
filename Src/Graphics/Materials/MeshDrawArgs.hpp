#pragma once

#include "../DeferredRenderer.hpp"
#include "../Lighting/PointLightShadowMapper.hpp"

enum class MeshDrawMode
{
	Game,
	BlurredGlassDepthOnly,
	TransparentBeforeWater,
	TransparentBeforeBlur,
	TransparentFinal,
	Emissive,
	Editor,
	PointLightShadow,
	AdditionalSSR
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
