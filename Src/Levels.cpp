#include "Levels.hpp"

#include <filesystem>

static eg::LinearAllocator levelsMemAllocator;

void InitLevels()
{
	std::string levelsPath = eg::ExeRelPath("levels");
	if (!eg::FileExists(levelsPath.c_str()))
	{
		eg::CreateDirectory(levelsPath.c_str());
	}
	else
	{
		for (auto entry : std::filesystem::directory_iterator(levelsPath))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".gwd")
				continue;
			
			Level* level = levelsMemAllocator.New<Level>();
			level->name = levelsMemAllocator.MakeStringCopy(entry.path().stem().string());
			level->next = firstLevel;
			firstLevel = level;
		}
	}
}

Level* FindLevel(std::string_view name)
{
	for (Level* level = firstLevel; level != nullptr; level = level->next)
	{
		if (level->name == name)
			return level;
	}
	return nullptr;
}
