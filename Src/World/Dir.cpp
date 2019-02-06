#include "Dir.hpp"

const char* DirectionNames[6] = 
{
	"+x", "-x", "+y", "-y", "+z", "-z"
};

std::optional<Dir> ParseDirection(std::string_view directionName)
{
	for (int s = 0; s < 6; s++)
	{
		if (directionName == DirectionNames[s])
			return (Dir)s;
	}
	return { };
}
