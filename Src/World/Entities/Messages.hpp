#pragma once

#include <functional>

struct DrawMessage : eg::Message<DrawMessage>
{
	eg::MeshBatch* meshBatch;
	std::vector<struct ReflectionPlane*>* reflectionPlanes;
	const class World* world;
};

struct CalculateCollisionMessage : eg::Message<CalculateCollisionMessage>
{
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
