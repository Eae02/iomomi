#include "Clipping.hpp"
#include "World.hpp"

void CalcPolygonClipping(ClippingArgs& args, eg::Span<const glm::vec3> vertices)
{
	std::vector<uint32_t> indices;
	for (uint32_t i = 2; i < vertices.size(); i++)
	{
		indices.push_back(0);
		indices.push_back(i - 1);
		indices.push_back(i);
	}
	
	eg::CollisionMesh mesh = eg::CollisionMesh::CreateV3<uint32_t>(vertices, indices);
	//mesh.FlipWinding();
	eg::CheckEllipsoidMeshCollision(args.collisionInfo, args.ellipsoid, args.move, mesh, glm::mat4(1.0f));
}
