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
		eg::ReleasePanic("Missing directory \"levels\".");
	}
	
	levels.clear();
	
	//Adds all gwd files in the levels directory to the levels list
	for (const auto& entry : directory_iterator(levelsPath))
	{
		if (is_regular_file(entry.status()) && entry.path().extension() == ".gwd")
		{
			levels.push_back({entry.path().stem().string()});
		}
	}
	
	//Sorts levels by name so that these can be binary searched over later
	std::sort(levels.begin(), levels.end(), [&] (const Level& a, const Level& b)
	{
		return a.name < b.name;
	});
}

void InitLevelsGraph()
{
	YAML::Node graphRoot;
	try
	{
		graphRoot = YAML::LoadFile(eg::ExeRelPath("levels/graph.yaml"));
	}
	catch (...)
	{
		eg::ReleasePanic("Cannot open file \"levels/graph.yaml\" for reading.");
	}
	
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

int64_t FindLevel(std::string_view name)
{
	auto it = std::lower_bound(levels.begin(), levels.end(), name, [&] (const Level& a, std::string_view b)
	{
		return a.name < b;
	});
	
	if (it == levels.end() || it->name != name)
		return -1;
	return it - levels.begin();
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
