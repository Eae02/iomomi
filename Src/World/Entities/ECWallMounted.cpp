#include "ECWallMounted.hpp"
#include "../Voxel.hpp"

glm::mat4 ECWallMounted::GetTransform(const eg::Entity& entity, float scale)
{
	return glm::translate(glm::mat4(1), eg::GetEntityPosition(entity)) *
	       glm::mat4(GetRotationMatrix(entity)) *
	       glm::scale(glm::mat4(1), glm::vec3(scale));
}

glm::mat3 ECWallMounted::GetRotationMatrix(const eg::Entity& entity)
{
	const Dir up = entity.GetComponent<ECWallMounted>().wallUp;
	const glm::vec3 l = voxel::tangents[(int)up];
	const glm::vec3 u = DirectionVector(up);
	return glm::mat3(l, u, glm::cross(l, u));
}

eg::AABB ECWallMounted::GetAABB(const eg::Entity& entity, float scale, float upDist)
{
	const Dir up = entity.GetComponent<ECWallMounted>().wallUp;
	
	glm::vec3 diag(scale);
	diag[(int)up / 2] = 0;
	
	glm::vec3 position = eg::GetEntityPosition(entity);
	return eg::AABB(position - diag, position + diag + glm::vec3(DirectionVector(up)) * (upDist * scale));
}
