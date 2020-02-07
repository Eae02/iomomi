#include "AxisAlignedQuadComp.hpp"

std::tuple<glm::vec3, glm::vec3> AxisAlignedQuadComp::GetTangents(float rotation) const
{
	glm::vec3 upAxis = GetNormal();
	
	glm::vec3 size3(1);
	size3[(upPlane + 1) % 3] = size.x;
	size3[(upPlane + 2) % 3] = size.y;
	
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
