#pragma once

enum class Dir
{
	PosX,
	NegX,
	PosY,
	NegY,
	PosZ,
	NegZ
};

inline glm::ivec3 DirectionVector(Dir d)
{
	glm::vec3 v(0.0f);
	v[(int)d / 2] = (int)d % 2 == 0 ? 1 : -1;
	return v;
}

inline Dir OppositeDir(Dir d)
{
	int id = (int)d;
	return (Dir)(id - 2 * (id % 2) + 1);
}
