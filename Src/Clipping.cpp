#include "Clipping.hpp"

std::optional<glm::vec3> CollisionCorrection(const eg::AABB& aabb, const glm::vec3* triangleVertices)
{
	
}

std::optional<glm::vec3> CollisionCorrection(const eg::AABB& aabb,
	const eg::CollisionMesh& mesh, const glm::mat4& meshTransform)
{
	for (uint32_t i = 0; i < mesh.NumIndices(); i += 3)
	{
		glm::vec3 vertices[3];
		for (uint32_t j = 0; j < 3; j++)
		{
			vertices[j] = meshTransform * glm::vec4(mesh.VertexByIndex(i + j), 1);
		}
		std::optional<glm::vec3> correction = CollisionCorrection(aabb, vertices);
		if (correction.has_value())
			return correction;
	}
	return { };
}
