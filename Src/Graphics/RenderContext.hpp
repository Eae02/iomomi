#pragma once

#include "RenderSettings.hpp"
#include "ObjectRenderer.hpp"
#include "Renderer.hpp"

struct RenderContext
{
	eg::PerspectiveProjection projection;
	ObjectRenderer objectRenderer;
	Renderer renderer;
};
