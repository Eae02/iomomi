#include "Levels.hpp"

#ifdef __EMSCRIPTEN__
#include <experimental/filesystem>
using namespace std::experimental::filesystem;
#else
#include <filesystem>
using namespace std::filesystem;
#endif
#include <yaml-cpp/yaml.h>

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
		
		for (auto entry : directory_iterator(levelsPath))
		{
			if (!is_regular_file(entry.status()) || entry.path().extension() != ".gwd")
				continue;
			
			levels.push_back({ entry.path().stem().string() });
		}
		
		std::sort(levels.begin(), levels.end(), [&] (const Level& a, const Level& b)
		{
			return a.name < b.name;
		});
		
		YAML::Node graphRoot = YAML::LoadFile(levelsPath + "/graph.yaml");
		for (auto levelIt = graphRoot.begin(); levelIt != graphRoot.end(); ++levelIt)
		{
			std::string srcLevelName = levelIt->first.as<std::string>();
			int64_t srcLevel = FindLevel(srcLevelName);
			if (srcLevel == -1)
			{
				eg::Log(eg::LogLevel::Error, "lvl", "Level not found: {0}", srcLevelName);
				continue;
			}
			
			auto AddNextLevel = [&] (const std::string& levelName, std::string_view exitName)
			{
				const uint32_t exitNameHash = eg::HashFNV1a32(exitName);
				int64_t nextLevel = FindLevel(levelName);
				if (srcLevel == -1)
				{
					eg::Log(eg::LogLevel::Error, "lvl", "Level not found: {0}", levelName);
				}
				else
				{
					levels[srcLevel].nextLevels.push_back({ nextLevel, exitNameHash });
				}
			};
			
			if (levelIt->second.IsMap())
			{
				for (auto edgeIt = levelIt->second.begin(); edgeIt != levelIt->second.end(); ++edgeIt)
				{
					AddNextLevel(edgeIt->second.as<std::string>(), edgeIt->first.as<std::string>());
				}
			}
			else
			{
				AddNextLevel(levelIt->second.as<std::string>(), "main");
			}
		}
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

int64_t GetNextLevelIndex(int64_t currentLevel, std::string_view exitName)
{
	if (currentLevel < 0 || currentLevel >= (int64_t)levels.size())
		return -1;
	
	uint32_t exitNameHash = eg::HashFNV1a32(exitName);
	
	for (const NextLevel& level : levels[currentLevel].nextLevels)
	{
		if (level.exitNameHash == exitNameHash)
			return level.nextIndex;
	}
	
	return -1;
}

std::string GetLevelPath(std::string_view name)
{
	return eg::Concat({ eg::ExeDirPath(), "/levels/", name, ".gwd" });
}
