#pragma once

#include <string_view>

struct Level
{
	std::string_view name;
	Level* next;
};

extern Level* firstLevel;

void InitLevels();

Level* FindLevel(std::string_view name);
