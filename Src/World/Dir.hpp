#pragma once

enum class Dir
{
	PosX = 0,
	NegX = 1,
	PosY = 2,
	NegY = 3,
	PosZ = 4,
	NegZ = 5
};

extern const char* DirectionNames[6];

inline const char* DirectionName(Dir d)
{
	return DirectionNames[static_cast<int>(d)];
}

std::optional<Dir> ParseDirection(std::string_view directionName);

inline glm::ivec3 DirectionVector(Dir d)
{
	glm::vec3 v(0.0f);
	v[static_cast<int>(d) / 2] = static_cast<int>(d) % 2 == 0 ? 1 : -1;
	return v;
}

inline Dir OppositeDir(Dir d)
{
	int id = static_cast<int>(d);
	return static_cast<Dir>(id - 2 * (id % 2) + 1);
}

namespace voxel
{
extern const glm::ivec3 tangents[6];

extern const glm::ivec3 biTangents[6];
} // namespace voxel
