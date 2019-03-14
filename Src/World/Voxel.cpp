#include "Voxel.hpp"

namespace voxel
{
	const glm::ivec3 tangents[6] =
	{
		{ 0, 1, 0 },
		{ 0, 1, 0 },
		{ 1, 0, 0 },
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 1, 0 }
	};
	
	const glm::ivec3 biTangents[6] =
	{
		{ 0, 0, -1 },
		{ 0, 0, 1 },
		{ 0, 0, 1 },
		{ 0, 0, -1 },
		{ 1, 0, 0 },
		{ -1, 0, 0 }
	};
}
