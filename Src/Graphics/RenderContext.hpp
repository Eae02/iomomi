#pragma once

#include "DeferredRenderer.hpp"
#include "ParticleRenderer.hpp"
#include "PostProcessor.hpp"
#include "RenderSettings.hpp"
#include "SSR.hpp"

struct RenderContext
{
	eg::MeshBatch meshBatch;
	eg::MeshBatchOrdered transparentMeshBatch;
	DeferredRenderer renderer;
	WaterRenderer waterRenderer;
	SSR ssr;
	PostProcessor postProcessor;
	ParticleRenderer particleRenderer;
};
