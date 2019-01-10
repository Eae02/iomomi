#include "Voxel.hpp"

namespace voxel
{
	const uint8_t opposingSides[6] = 
	{
		/* POSX -> */ NEGX,
		/* NEGX -> */ POSX,
		/* POSY -> */ NEGY,
		/* NEGY -> */ POSY,
		/* POSZ -> */ NEGZ,
		/* NEGZ -> */ POSZ
	};
	
	const glm::ivec3 normals[6] =
	{
		{  1, 0, 0 },
		{ -1, 0, 0 },
		{ 0,  1, 0 },
		{ 0, -1, 0 },
		{ 0, 0,  1 },
		{ 0, 0, -1 }
	};
	
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
