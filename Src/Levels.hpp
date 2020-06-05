#pragma once

#include <string_view>

struct Level
{
	std::string name;
	int64_t nextLevelIndex = -1;
};

extern std::vector<Level> levels;

void InitLevels();

std::string GetLevelPath(std::string_view name);

int64_t FindLevel(std::string_view name);
