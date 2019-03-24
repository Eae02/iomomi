#pragma once

#include "RenderSettings.hpp"
#include "DeferredRenderer.hpp"

struct RenderContext
{
	eg::MeshBatch meshBatch;
	DeferredRenderer renderer;
};
