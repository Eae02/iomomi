#pragma once

void VecEncode(const glm::vec3& v, int8_t* out);

struct WallVertex
{
	float position[3];
	float texCoord[3];
	int8_t normalAndRoughnessLo[4];
	int8_t tangentAndRoughnessHi[4];

	void SetNormal(const glm::vec3& _normal) { VecEncode(_normal, normalAndRoughnessLo); }

	void SetTangent(const glm::vec3& _tangent) { VecEncode(_tangent, tangentAndRoughnessHi); }
};

struct WallBorderVertex
{
	glm::vec4 position;
	int8_t normal1[4];
	int8_t normal2[4];
};
