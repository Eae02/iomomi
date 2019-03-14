#include "Clipping.hpp"

void CalcPolygonClipping(ClippingArgs& args, eg::Span<const glm::vec3> vertices)
{
	EG_ASSERT(vertices.size() >= 3);
	eg::Plane plane(vertices[0], vertices[1], vertices[2]);
	
	glm::vec3 aabbVertices[] =
	{
		glm::vec3(args.aabbMin.x, args.aabbMin.y, args.aabbMin.z),
		glm::vec3(args.aabbMin.x, args.aabbMin.y, args.aabbMax.z),
		glm::vec3(args.aabbMin.x, args.aabbMax.y, args.aabbMin.z),
		glm::vec3(args.aabbMin.x, args.aabbMax.y, args.aabbMax.z),
		glm::vec3(args.aabbMax.x, args.aabbMin.y, args.aabbMin.z),
		glm::vec3(args.aabbMax.x, args.aabbMin.y, args.aabbMax.z),
		glm::vec3(args.aabbMax.x, args.aabbMax.y, args.aabbMin.z),
		glm::vec3(args.aabbMax.x, args.aabbMax.y, args.aabbMax.z),
	};
	
	glm::vec3 centerAABB = (args.aabbMin + args.aabbMax) / 2.0f;
	
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
