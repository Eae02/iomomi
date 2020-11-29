#pragma once

#include "RenderSettings.hpp"
#include "DeferredRenderer.hpp"
#include "SSR.hpp"
#include "PostProcessor.hpp"
#include "ParticleRenderer.hpp"

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
