#pragma once

#include <functional>

struct DrawMessage : eg::Message<DrawMessage>
{
	eg::MeshBatch* meshBatch;
};

struct CalculateCollisionMessage : eg::Message<CalculateCollisionMessage>
{
	struct ClippingArgs* clippingArgs;
};
