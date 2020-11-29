#pragma once

#include <string_view>

enum class LevelStatus
{
	Locked,
	Unlocked,
	Completed
};

struct Level
{
	std::string name;
	LevelStatus status = LevelStatus::Unlocked;
	bool isExtra = true;
	int64_t nextLevelIndex = -1;
	eg::Texture thumbnail;
};

extern std::vector<Level> levels;

extern const std::vector<std::string_view> levelsOrder;

void MarkLevelCompleted(Level& level);

void InitLevels();

void SaveProgress();

void LoadLevelThumbnail(Level& level);

std::string GetLevelPath(std::string_view name);
std::string GetLevelThumbnailPath(std::string_view name);

int64_t FindLevel(std::string_view name);
