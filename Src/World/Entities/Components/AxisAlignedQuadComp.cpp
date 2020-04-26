#include "AxisAlignedQuadComp.hpp"
#include "../../Dir.hpp"
#include "../../Collision.hpp"

std::tuple<glm::vec3, glm::vec3> AxisAlignedQuadComp::GetTangents(float rotation) const
{
	glm::vec3 upAxis = GetNormal();
	
	glm::vec3 size3(1);
	size3[(upPlane + 1) % 3] = radius.x;
	size3[(upPlane + 2) % 3] = radius.y;
	
	glm::vec3 tangent(0);
	glm::vec3 bitangent(0);
	
	tangent[(upPlane + 1) % 3] = 1;
	bitangent[(upPlane + 2) % 3] = 1;
	
	glm::mat3 rotationMatrix = glm::rotate(glm::mat4(1), rotation, upAxis);
	tangent = (rotationMatrix * tangent) * size3;
	bitangent = (rotationMatrix * bitangent) * size3;
	
	return std::make_tuple(tangent, bitangent);
}

glm::vec3 AxisAlignedQuadComp::GetNormal() const
{
	glm::vec3 upAxis(0);
	upAxis[upPlane] = 1;
	return upAxis;
}

AxisAlignedQuadComp::CollisionGeometry AxisAlignedQuadComp::GetCollisionGeometry(
	const glm::vec3& center, float depth) const
{
	auto [tangent, bitangent] = GetTangents(0);
	glm::vec3 normal = GetNormal() * depth;
	glm::vec3 boxSize = (tangent + bitangent) * 0.5f + normal;
	
	CollisionGeometry geometry;
	
	for (int f = 0; f < 6; f++)
	{
		glm::vec3 faceCenter = center + glm::vec3(DirectionVector((Dir)f)) * boxSize;
		for (int a = 0; a < 2; a++)
		{
			for (int b = 0; b < 2; b++)
			{
				geometry.vertices[f][a * 2 + b] = faceCenter +
					glm::vec3(voxel::tangents[f] * (a * 2 - 1) + voxel::biTangents[f] * (b * 2 - 1)) * boxSize;
			}
		}
	}
	
	return geometry;
}

std::optional<glm::vec3> AxisAlignedQuadComp::CollisionGeometry::CheckCollision(
	const eg::AABB& aabb, const glm::vec3& moveDir) const
{
	CollisionResponseCombiner combiner;
	
	for (int f = 0; f < 6; f++)
	{
		combiner.Update(CheckCollisionAABBPolygon(aabb, vertices[f], moveDir));
	}
	
	return combiner.GetCorrection();
}
