#include "Clipping.hpp"

void CalcPolygonClipping(ClippingArgs& args, eg::Span<const glm::vec3> vertices)
{
	EG_ASSERT(vertices.size() >= 3);
	eg::Plane plane(vertices[0], vertices[1], vertices[2]);
	
	glm::vec3 aabbVertices[] =
	{
		glm::vec3(args.aabb.min.x, args.aabb.min.y, args.aabb.min.z),
		glm::vec3(args.aabb.min.x, args.aabb.min.y, args.aabb.max.z),
		glm::vec3(args.aabb.min.x, args.aabb.max.y, args.aabb.min.z),
		glm::vec3(args.aabb.min.x, args.aabb.max.y, args.aabb.max.z),
		glm::vec3(args.aabb.max.x, args.aabb.min.y, args.aabb.min.z),
		glm::vec3(args.aabb.max.x, args.aabb.min.y, args.aabb.max.z),
		glm::vec3(args.aabb.max.x, args.aabb.max.y, args.aabb.min.z),
		glm::vec3(args.aabb.max.x, args.aabb.max.y, args.aabb.max.z),
	};
	
	glm::vec3 centerAABB = (args.aabb.min + args.aabb.max) / 2.0f;
	
	float div = glm::dot(args.move, plane.GetNormal());
	if (std::abs(div) < 1E-6f)
		return;
	if (div > 0)
	{
		plane.FlipNormal();
		div = -div;
	}
	
	for (const glm::vec3& v : aabbVertices)
	{
		if (glm::dot(v - centerAABB, args.move) < 0)
			continue;
		
		float a = (plane.GetDistance() - glm::dot(v, plane.GetNormal())) / div;
		if (a >= -1E-4f && a < args.clipDist)
		{
			glm::vec3 iPos = plane.GetClosestPointOnPlane(v + a * args.move);
			bool inPolygon = false;
			for (size_t i = 2; i < vertices.size(); i++)
			{
				if (eg::TriangleContainsPoint(vertices[0], vertices[i - 1], vertices[i], iPos))
				{
					inPolygon = true;
					break;
				}
			}
			
			if (inPolygon)
			{
				args.clipDist = std::max(a, 0.0f);
				args.colPlaneNormal = plane.GetNormal();
			}
		}
	}
}

float CalcCollisionCorrection(const eg::AABB& aabb, const eg::Plane& plane)
{
	constexpr float MAX_C = 0.1f;
	
	float minD = 0;
	float maxD = 0;
	for (int n = 0; n < 8; n++)
	{
		float d = plane.GetDistanceToPoint(aabb.NthVertex(n));
		minD = std::min(minD, d);
		maxD = std::max(maxD, d);
	}
	float c = -minD;
	return (c > maxD || c > MAX_C) ? 0 : c;
}
