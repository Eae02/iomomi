#pragma once

#include "Entity.hpp"

struct EntRayIntersectArgs
{
	eg::Ray ray;
	Ent* entity = nullptr;
	float distance = INFINITY;
};

class EntCollidable
{
public:
	virtual void RayIntersect(EntRayIntersectArgs& args) const = 0;
	virtual void CalculateCollision(Dir currentDown, struct ClippingArgs& args) const = 0;
};
