#pragma once

struct ClippingArgs
{
	//Inputs:
	eg::AABB aabb;
	glm::vec3 move;
	
	//Outputs:
	glm::vec3 colPlaneNormal;
	float clipDist;
};

/***
 * Calculates clipping against a convex polygon.
 * The points must be coplanar and sorted by angle!
 * @param args Clipping arguments.
 * @param vertices The points that make up the polygon.
 */
void CalcPolygonClipping(ClippingArgs& args, eg::Span<const glm::vec3> vertices);

void CalcWorldClipping(const class World& world, ClippingArgs& args);

float CalcCollisionCorrection(const eg::AABB& aabb, const eg::Plane& plane);

glm::vec3 CalcWorldCollisionCorrection(const class World& world, const eg::AABB& aabb);
