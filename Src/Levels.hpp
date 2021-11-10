#pragma once

#include <string_view>
#include "World/World.hpp"

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
	
	explicit Level(std::string _name = "")
		: name(std::move(_name)) { }
};

extern std::vector<Level> levels;

extern std::vector<std::string_view> levelsOrder;

std::unique_ptr<World> LoadLevelWorld(const Level& level, bool isEditor);

void MarkLevelCompleted(Level& level);

void InitLevels();

void ResetProgress();
void SaveProgress();

void LoadLevelThumbnail(Level& level);

std::string GetLevelPath(std::string_view name);
std::string GetLevelThumbnailPath(std::string_view name);

int64_t FindLevel(std::string_view name);

void SortLevels();

bool IsLevelLoadingComplete(std::string_view name);
