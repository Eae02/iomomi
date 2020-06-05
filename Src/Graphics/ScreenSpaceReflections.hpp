#pragma once

#include "DeferredRenderer.hpp"

class ScreenSpaceReflections
{
public:
	ScreenSpaceReflections();
	
	void Render(DeferredRenderer::RenderTarget& renderTarget);
	
private:
	eg::Texture m_reflectionTexture;
	eg::Framebuffer m_reflectionFB;
};
