#include "Dir.hpp"

const char* DirectionNames[6] = 
{
	"+X", "-X", "+Y", "-Y", "+Z", "-Z"
};

std::optional<Dir> ParseDirection(std::string_view directionName)
{
	for (int s = 0; s < 6; s++)
	{
		if (directionName == DirectionNames[s])
			return static_cast<Dir>(s);
	}
	return { };
}

const glm::ivec3 voxel::tangents[6] =
{
	{ 0, 1, 0 },
	{ 0, 1, 0 },
	{ 1, 0, 0 },
	{ 1, 0, 0 },
	{ 0, 1, 0 },
	{ 0, 1, 0 }
};

const glm::ivec3 voxel::biTangents[6] =
{
	{ 0, 0, -1 },
	{ 0, 0, 1 },
	{ 0, 0, 1 },
	{ 0, 0, -1 },
	{ 1, 0, 0 },
	{ -1, 0, 0 }
};
