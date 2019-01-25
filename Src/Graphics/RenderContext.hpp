#pragma once

#include "RenderSettings.hpp"
#include "ObjectRenderer.hpp"

struct RenderContext
{
	eg::PerspectiveProjection projection;
	RenderSettings renderSettings;
	ObjectRenderer objectRenderer;
};
