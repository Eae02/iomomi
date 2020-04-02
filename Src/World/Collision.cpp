#include "Collision.hpp"

#pragma GCC optimize("Ofast")

std::optional<glm::vec3> CheckCollisionAABBPolygon(const eg::AABB& aabb, eg::Span<const glm::vec3> polyVertices,
	const glm::vec3& moveDir, float shiftAmount)
{
	glm::vec3 planeNormal = glm::cross(polyVertices[1] - polyVertices[0], polyVertices[2] - polyVertices[0]);
	if (glm::dot(planeNormal, moveDir) > 0)
		return { }; //Moving in same direction as normal, so no collision
	planeNormal = glm::normalize(planeNormal);
	glm::vec3 shift = planeNormal * shiftAmount;
	
	//Checks the AABB's planes
	for (int axis = 0; axis < 3; axis++)
	{
		float triMin = INFINITY;
		float triMax = -INFINITY;
		for (const glm::vec3& polyVertex : polyVertices)
		{
			float d = polyVertex[axis] + shift[axis];
			triMin = std::min(triMin, d);
			triMax = std::max(triMax, d);
		}
		
		if (triMin > aabb.max[axis] - 1E-5f || triMax < aabb.min[axis] + 1E-5f)
		{
			//No collision (SAT)
			return {};
		}
	}
	
	//Checks the triangle's planes
	float planeDist = glm::dot(planeNormal, polyVertices[0] + shift);
	float minDist = INFINITY;
	float maxDist = -INFINITY;
	auto CheckVertex = [&] (float x, float y, float z)
	{
		float d = planeNormal.x * x + planeNormal.y * y + planeNormal.z * z - planeDist;
		minDist = std::min(minDist, d);
		maxDist = std::max(maxDist, d);
	};
	CheckVertex(aabb.min.x, aabb.min.y, aabb.min.z);
	CheckVertex(aabb.max.x, aabb.min.y, aabb.min.z);
	CheckVertex(aabb.min.x, aabb.max.y, aabb.min.z);
	CheckVertex(aabb.max.x, aabb.max.y, aabb.min.z);
	CheckVertex(aabb.min.x, aabb.min.y, aabb.max.z);
	CheckVertex(aabb.max.x, aabb.min.y, aabb.max.z);
	CheckVertex(aabb.min.x, aabb.max.y, aabb.max.z);
	CheckVertex(aabb.max.x, aabb.max.y, aabb.max.z);
	
	if (minDist >= 0 || maxDist <= 0)
	{
		//No collision (SAT)
		return { };
	}
	
	/* The situation looks like this:
	    _____
	    |   | <--- AABB
	  __|___|__ <- Triangle (normal facing up)
	    |___|
	  The AABB should be moved up by |minDist|
	 */
	return planeNormal * std::abs(minDist);
}

std::optional<glm::vec3> CheckCollisionAABBTriangleMesh(const eg::AABB& aabb, const glm::vec3& moveDir,
	const eg::CollisionMesh& mesh, const glm::mat4& meshTransform)
{
	if (!aabb.Intersects(mesh.BoundingBox().TransformedBoundingBox(meshTransform)))
		return { };
	
	CollisionResponseCombiner combiner;
	
	for (uint32_t i = 0; i < mesh.NumIndices(); i += 3)
	{
		glm::vec3 vertices[3];
		for (uint32_t j = 0; j < 3; j++)
		{
			vertices[j] = glm::vec3(meshTransform * glm::vec4(mesh.VertexByIndex(i + j), 1.0f));
		}
		combiner.Update(CheckCollisionAABBPolygon(aabb, vertices, moveDir));
	}
	return combiner.GetCorrection();
}

void CollisionResponseCombiner::Update(const glm::vec3& correction)
{
	float mag2 = glm::length2(correction);
	if (mag2 > 1E-5f && mag2 < m_correctionMag2)
	{
		m_correctionMag2 = mag2;
		m_correction = correction;
	}
}
