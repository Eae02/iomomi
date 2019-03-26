#pragma once

#include <string_view>

struct Level
{
	std::string name;
};

extern std::vector<Level> levels;

void InitLevels();

int64_t FindLevel(std::string_view name);
