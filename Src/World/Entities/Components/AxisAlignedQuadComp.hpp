#pragma once

struct AxisAlignedQuadComp
{
	int upPlane = 0;
	glm::vec2 size { 1.0f };
	
	glm::vec3 GetNormal() const;
	
	std::tuple<glm::vec3, glm::vec3> GetTangents(float rotation) const;
};
