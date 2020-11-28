#include "Levels.hpp"
#include "World/World.hpp"

#ifdef __EMSCRIPTEN__
#include <experimental/filesystem>
using namespace std::experimental::filesystem;
#else
#include <filesystem>
#include <fstream>

using namespace std::filesystem;
#endif

std::vector<Level> levels;

int64_t firstLevelIndex = -1;

const std::vector<std::string_view> levelsOrder = 
{
	//PART 1
	"intro_0",
	"intro_1",
	"intro_2",
	"button_0",
	"button_1",
	"button_2",
	"button_3",
	
	"gravbarrier_ng0",
	"gravbarrier_ng1",
    "gravbarrier_ng2",
	"gravbarrier_ng3",
	"gravbarrier_ng4",
	
	"water_ng0",
	"water_ng1",
	"water_ng2",
	"water_ng3",
	"water_ng4",
	"water_ng5",
	
	//PART 2
	"gravgun_0",
	"gravgun_1",
	"gravgun_2",
	"gravgun_3",
	"gravgun_4",
	"gravgun_5",
	"gravgun_6",
	
	"forcefield_1",
	"forcefield_2",
	"forcefield_3",
	//"forcefield_4",
	
	//PART 3
	"gravbarrier_1",
	"gravbarrier_2",
	"gravbarrier_3",
	"gravbarrier_4",
	
	"water_0",
	"water_1",
	"water_2",
	
	"water_4",
	"water_5",
	"water_6"
};

void UpgradeLevelsCommand(eg::Span<const std::string_view> args, eg::console::Writer& writer)
{
	int numUpgraded = 0;
	for (const Level& level : levels)
	{
		std::string path = GetLevelPath(level.name);
		
		std::ifstream inputStream(path, std::ios::binary);
		if (!inputStream)
		{
			std::string message = "Could not open " + path + "!";
			writer.WriteLine(eg::console::InfoColor, message);
		}
		else
		{
			std::unique_ptr<World> world = World::Load(inputStream, false);
			inputStream.close();
			
			if (world->IsLatestVersion())
				continue;
			
			std::string text = "Upgrading " + path + "...";
			writer.WriteLine(eg::console::InfoColor, text);
			
			std::ofstream outStream(path, std::ios::binary);
			world->Save(outStream);
			
			numUpgraded++;
		}
	}
	
	std::string endMessage = "Upgraded " + std::to_string(numUpgraded) + " level(s)";
	writer.WriteLine(eg::console::InfoColor, endMessage);
}

void MarkLevelCompleted(Level& level)
{
	if (level.status != LevelStatus::Completed)
	{
		level.status = LevelStatus::Completed;
		if (level.nextLevelIndex != -1 && levels[level.nextLevelIndex].status == LevelStatus::Locked)
		{
			levels[level.nextLevelIndex].status = LevelStatus::Unlocked;
		}
		SaveProgress();
	}
}

static std::string progressPath;

void InitLevels()
{
	eg::console::AddCommand("upgradeLevels", 0, &UpgradeLevelsCommand);
	
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
	
	//Sets the next level index
	int64_t levelIndex = FindLevel(levelsOrder[0]);
	for (size_t i = 0; i < std::size(levelsOrder); i++)
	{
		int64_t nextLevelIndex = i + 1 == std::size(levelsOrder) ? -1 : FindLevel(levelsOrder[i + 1]);
		if (levelIndex == -1)
		{
			eg::Log(eg::LogLevel::Error, "lvl", "Builtin level {0} not found", levelsOrder[i]);
		}
		else
		{
			levels[levelIndex].status = i == 0 ? LevelStatus::Unlocked : LevelStatus::Locked;
			levels[levelIndex].nextLevelIndex = nextLevelIndex;
			levels[levelIndex].isExtra = false;
		}
		levelIndex = nextLevelIndex;
	}
	
	//Loads the progress file if it exists
	progressPath = eg::AppDataPath() + "EaeGravity/Progress.txt";
	std::ifstream progressFileStream(progressPath);
	if (progressFileStream)
	{
		std::string line;
		while (std::getline(progressFileStream, line))
		{
			int64_t index = FindLevel(eg::TrimString(line));
			if (index != -1)
			{
				levels[index].status = LevelStatus::Completed;
				if (levels[index].nextLevelIndex != -1 && levels[levels[index].nextLevelIndex].status == LevelStatus::Locked)
				{
					levels[levels[index].nextLevelIndex].status = LevelStatus::Unlocked;
				}
			}
		}
	}
}

void SaveProgress()
{
	std::ofstream progressFileStream(progressPath);
	if (!progressFileStream)
	{
		eg::Log(eg::LogLevel::Error, "io", "Failed to save progress!");
		return;
	}
	
	for (const Level& level : levels)
	{
		if (level.status == LevelStatus::Completed)
		{
			progressFileStream << level.name << "\n";
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

std::string GetLevelPath(std::string_view name)
{
	return eg::Concat({ eg::ExeDirPath(), "/levels/", name, ".gwd" });
}
