#include "Vertex.hpp"

void Vertex::VecEncode(const glm::vec3& v, int8_t* out)
{
	float scale = 127.0f / glm::length(v);
	for (int i = 0; i < 3; i++)
	{
		out[i] = static_cast<int8_t>(v[i] * scale);
	}
}
