#include "Levels.hpp"

#include <filesystem>

std::vector<Level> levels;

void InitLevels()
{
	std::string levelsPath = eg::ExeRelPath("levels");
	if (!eg::FileExists(levelsPath.c_str()))
	{
		eg::CreateDirectory(levelsPath.c_str());
	}
	else
	{
		levels.clear();
		
		for (auto entry : std::filesystem::directory_iterator(levelsPath))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".gwd")
				continue;
			
			levels.push_back({ entry.path().stem().string() });
		}
		
		std::sort(levels.begin(), levels.end(), [&] (const Level& a, const Level& b)
		{
			return a.name < b.name;
		});
	}
}

int64_t FindLevel(std::string_view name)
{
	for (size_t i = 0; i < levels.size(); i++)
	{
		if (levels[i].name == name)
			return i;
	}
	return -1;
}
