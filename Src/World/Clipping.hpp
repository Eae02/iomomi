#pragma once

struct ClippingArgs
{
	eg::CollisionEllipsoid ellipsoid;
	eg::CollisionInfo collisionInfo;
	glm::vec3 move;
};

/***
 * Calculates clipping against a convex polygon.
 * The points must be coplanar and sorted by angle!
 * @param args Clipping arguments.
 * @param vertices The points that make up the polygon.
 */
void CalcPolygonClipping(ClippingArgs& args, eg::Span<const glm::vec3> vertices);
