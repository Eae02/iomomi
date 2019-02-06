#pragma once

#include "RenderSettings.hpp"
#include "ObjectRenderer.hpp"
#include "Renderer.hpp"

struct RenderContext
{
	ObjectRenderer objectRenderer;
	Renderer renderer;
};
