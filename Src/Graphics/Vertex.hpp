#pragma once

void VecEncode(const glm::vec3& v, int8_t* out);

struct WallVertex
{
	glm::vec3 position;
	uint8_t texCoord[4];
	int8_t normal[4];
	int8_t tangent[4];
	
	void SetNormal(const glm::vec3& _normal)
	{
		VecEncode(_normal, normal);
	}
	
	void SetTangent(const glm::vec3& _tangent)
	{
		VecEncode(_tangent, tangent);
	}
};

struct WallBorderVertex
{
	glm::vec3 position;
	int8_t normal1[4];
	int8_t normal2[4];
};
