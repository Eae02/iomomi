#pragma once

#include <functional>

#include "../Dir.hpp"

struct DrawMessage : eg::Message<DrawMessage>
{
	eg::MeshBatch* meshBatch;
	eg::MeshBatchOrdered* transparentMeshBatch;
	std::vector<struct ReflectionPlane*>* reflectionPlanes;
	const class World* world;
};

struct CalculateCollisionMessage : eg::Message<CalculateCollisionMessage>
{
	Dir currentDown;
	struct ClippingArgs* clippingArgs;
};

struct RayIntersectArgs
{
	eg::Ray ray;
	eg::Entity* entity = nullptr;
	float distance = INFINITY;
};

struct RayIntersectMessage : eg::Message<RayIntersectMessage>
{
	RayIntersectArgs* rayIntersectArgs;
};
