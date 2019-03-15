#pragma once

#include "RenderSettings.hpp"
#include "Renderer.hpp"

struct RenderContext
{
	eg::MeshBatch meshBatch;
	Renderer renderer;
};
