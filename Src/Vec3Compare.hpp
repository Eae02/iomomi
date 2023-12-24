#pragma once

struct Vec3Compare
{
	bool operator()(const glm::vec3& a, const glm::vec3& b) const
	{
		return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
	}
};

struct IVec3Compare
{
	bool operator()(const glm::ivec3& a, const glm::ivec3& b) const
	{
		return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
	}
};
