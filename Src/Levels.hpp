#pragma once

#include <string_view>

struct NextLevel
{
	int64_t nextIndex;
	uint32_t exitNameHash;
};

struct Level
{
	std::string name;
	std::vector<NextLevel> nextLevels;
};

extern std::vector<Level> levels;

void InitLevels();
void InitLevelsGraph();

std::string GetLevelPath(std::string_view name);

int64_t FindLevel(std::string_view name);

int64_t GetNextLevelIndex(int64_t currentLevel, std::string_view exitName);
