#pragma once

#include <functional>

struct DrawMessage : eg::Message<DrawMessage>
{
	eg::MeshBatch* meshBatch;
	const class World* world;
};

struct CalculateCollisionMessage : eg::Message<CalculateCollisionMessage>
{
	struct ClippingArgs* clippingArgs;
};
