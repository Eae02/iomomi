#pragma once

namespace voxel
{
	enum Side
	{
		POSX,
		NEGX,
		POSY,
		NEGY,
		POSZ,
		NEGZ
	};
	
	extern const uint8_t opposingSides[6];
	
	extern const glm::ivec3 normals[6];
	
	extern const glm::ivec3 tangents[6];
	
	extern const glm::ivec3 biTangents[6];
}
