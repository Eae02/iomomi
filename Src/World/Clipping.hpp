#pragma once

struct ClippingArgs
{
	//Inputs:
	glm::vec3 aabbMin;
	glm::vec3 aabbMax;
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
