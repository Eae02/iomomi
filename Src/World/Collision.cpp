#include "Collision.hpp"

#include "Dir.hpp"

std::optional<glm::vec3> CheckCollisionAABBPolygon(
	const eg::AABB& aabb, std::span<const glm::vec3> polyVertices, const glm::vec3& moveDir, float shiftAmount)
{
	glm::vec3 planeNormal = glm::cross(polyVertices[1] - polyVertices[0], polyVertices[2] - polyVertices[0]);
	if (glm::dot(planeNormal, moveDir) > 0)
		return {}; // Moving in same direction as normal, so no collision
	planeNormal = glm::normalize(planeNormal);
	glm::vec3 shift = planeNormal * shiftAmount;

	// Checks the AABB's planes
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
			// No collision (SAT)
			return {};
		}
	}

	// Checks the triangle's planes
	float planeDist = glm::dot(planeNormal, polyVertices[0] + shift);
	float minDist = INFINITY;
	float maxDist = -INFINITY;
	auto CheckVertex = [&](float x, float y, float z)
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
		// No collision (SAT)
		return {};
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

std::optional<glm::vec3> CheckCollisionAABBTriangleMesh(
	const eg::AABB& aabb, const glm::vec3& moveDir, const eg::CollisionMesh& mesh, const glm::mat4& meshTransform,
	bool flipWinding)
{
	if (!aabb.Intersects(mesh.BoundingBox().TransformedBoundingBox(meshTransform)))
		return {};

	CollisionResponseCombiner combiner;

	for (uint32_t i = 0; i < mesh.NumIndices(); i += 3)
	{
		glm::vec3 vertices[3];

		if (flipWinding)
		{
			vertices[0] = mesh.VertexByIndex(i + 2);
			vertices[1] = mesh.VertexByIndex(i + 1);
			vertices[2] = mesh.VertexByIndex(i + 0);
		}
		else
		{
			vertices[0] = mesh.VertexByIndex(i + 0);
			vertices[1] = mesh.VertexByIndex(i + 1);
			vertices[2] = mesh.VertexByIndex(i + 2);
		}

		for (uint32_t j = 0; j < 3; j++)
		{
			vertices[j] = glm::vec3(meshTransform * glm::vec4(vertices[j], 1.0f));
		}
		combiner.Update(CheckCollisionAABBPolygon(aabb, vertices, moveDir));
	}
	return combiner.GetCorrection();
}

std::optional<glm::vec3> CheckCollisionAABBOrientedBox(
	const eg::AABB& aabb, const OrientedBox& orientedBox, const glm::vec3& moveDir, float shiftAmount)
{
	CollisionResponseCombiner combiner;

	for (int f = 0; f < 6; f++)
	{
		glm::vec3 tangent = orientedBox.rotation * (glm::vec3(voxel::tangents[f]) * orientedBox.radius);
		glm::vec3 bitangent = orientedBox.rotation * (glm::vec3(voxel::biTangents[f]) * orientedBox.radius);
		glm::vec3 normal =
			orientedBox.rotation * (glm::vec3(DirectionVector(static_cast<Dir>(f))) * orientedBox.radius);
		glm::vec3 faceCenter = orientedBox.center + normal;

		glm::vec3 positions[] = { faceCenter - tangent - bitangent, faceCenter - tangent + bitangent,
			                      faceCenter + tangent - bitangent, faceCenter + tangent + bitangent };

		combiner.Update(CheckCollisionAABBPolygon(aabb, positions, moveDir, shiftAmount));
	}

	return combiner.GetCorrection();
}

std::optional<float> RayIntersectOrientedBox(const eg::Ray& ray, const OrientedBox& box)
{
	std::optional<float> ans;
	float minAns = INFINITY;
	for (int f = 0; f < 6; f++)
	{
		glm::vec3 normal = box.rotation * (glm::vec3(DirectionVector(static_cast<Dir>(f))) * box.radius);
		if (glm::dot(normal, ray.GetDirection()) >= 0)
			continue;

		glm::vec3 tangent = box.rotation * (glm::vec3(voxel::tangents[f]) * box.radius);
		glm::vec3 bitangent = box.rotation * (glm::vec3(voxel::biTangents[f]) * box.radius);
		glm::vec3 faceCenter = box.center + normal;
		eg::Plane plane(normal, faceCenter);

		// tangent /= glm::length2(tangent);
		// bitangent /= glm::length2(bitangent);

		float planeIntersect;
		if (ray.Intersects(plane, planeIntersect) && planeIntersect >= 0 && planeIntersect < minAns)
		{
			glm::vec3 intersectPointRel = ray.GetPoint(planeIntersect) - faceCenter;
			float distT = abs(glm::dot(intersectPointRel, tangent) / glm::length2(tangent));
			float distB = abs(glm::dot(intersectPointRel, bitangent) / glm::length2(bitangent));
			if (distT <= 1 && distB <= 1)
			{
				minAns = planeIntersect;
				ans = planeIntersect;
			}
		}
	}
	return ans;
}

bool CollisionResponseCombiner::Update(const glm::vec3& correction)
{
	float mag2 = glm::length2(correction);
	if (mag2 > 1E-5f && mag2 < m_correctionMag2)
	{
		m_correctionMag2 = mag2;
		m_correction = correction;
		return true;
	}
	return false;
}

OrientedBox OrientedBox::FromAABB(const eg::AABB& aabb)
{
	OrientedBox ob;
	ob.rotation = glm::quat();
	ob.radius = aabb.Size() / 2.0f;
	ob.center = aabb.Center();
	return ob;
}

OrientedBox OrientedBox::Transformed(const glm::mat4& matrix)
{
	OrientedBox ob;
	ob.center = matrix * glm::vec4(center, 1);

	glm::mat3 rs =
		glm::mat3(matrix) * glm::mat3_cast(rotation) * glm::mat3(radius.x, 0, 0, 0, radius.y, 0, 0, 0, radius.z);

	ob.radius.x = glm::length(rs[0]);
	ob.radius.y = glm::length(rs[1]);
	ob.radius.z = glm::length(rs[2]);

	rs[0] /= ob.radius.x;
	rs[1] /= ob.radius.y;
	rs[2] /= ob.radius.z;
	ob.rotation = glm::quat_cast(rs);

	return ob;
}
