#pragma once

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 position;
	glm::vec2 texCoord;
	int8_t normal[4];
	int8_t tangent[4];
	
	static void VecEncode(const glm::vec3& v, int8_t* out);
	
	void SetNormal(const glm::vec3& _normal)
	{
		VecEncode(_normal, normal);
	}
	void SetTangent(const glm::vec3& _tangent)
	{
		VecEncode(_tangent, tangent);
	}
};
