#include "Levels.hpp"

#include <filesystem>
#include <fstream>

#include "FileUtils.hpp"
#include "Water/WaterSimulationShaders.hpp"
#include "World/World.hpp"

using namespace std::filesystem;

std::vector<Level> levels;

std::vector<std::string_view> levelsOrder = {
	"intro_0",
	"intro_1",
	"intro_2",
	"intro_3",
	"button_0",
	"button_1",
	"button_2",
	"button_3",

	"gravbarrier_ng0",
	"gravbarrier_ng1",
	"gravbarrier_ng2",

	"water_ng0",
	"water_ng1",
	"water_ng2",
	"water_ng3",

	"gravbarrier_ng3",
	"water_pump_ng1",
	"water_pump_ng2",
	//"water_ng5",
	"gravbarrier_ng4",
	"gravbarrier_ng5",
	"water_pump_ng3",
	"gravbarrier_ng6",
	"gravbarrier_ng7",

	"gravgun_0",
	"gravgun_1",
	"gravgun_3",
	"gravgun_2",
	"gravgun_4",

	"gravbarrier_1",
	"gravbarrier_2",

	"water_0",
	"water_1",

	"forcefield_1",
	"forcefield_2",

	"gravbarrier_3",
	"gravbarrier_4",

	"forcefield_3",
	"forcefield_4",

	"gravgun_5",
	"gravgun_6",

	"water_2",
	"water_3",
	"water_4",
	"water_5",
	"water_6",

	"launch_0",
	"gravbarrier_cube_1",
	"launch_1",
	"gravbarrier_cube_2",
	"cubeflip_0",
	"launch_2",
	"gravbarrier_cube_3",
	"gravbarrier_cube_4",
	//"forcefield_5",
	"forcefield_6",

	"cubeflip_2",
};

static void OnShutdown()
{
	for (Level& level : levels)
	{
		level.thumbnail.Destroy();
	}
}

EG_ON_SHUTDOWN(OnShutdown)

void InitLevelsPlatformDependent();

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
std::string levelsDirPath;

void SortLevels()
{
	std::sort(levels.begin(), levels.end(), [&](const Level& a, const Level& b) { return a.name < b.name; });
}

void InitLevels()
{
	if (!waterSimShaders.isWaterSupported)
	{
		for (int64_t i = levelsOrder.size() - 1; i >= 0; i--)
		{
			if (levelsOrder[i].find("water") != std::string_view::npos)
				levelsOrder.erase(levelsOrder.begin() + i);
		}
	}

	levels.clear();
	InitLevelsPlatformDependent();

	// Sets the next level index
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
			levels[levelIndex].nextLevelIndex = nextLevelIndex;
			levels[levelIndex].isExtra = false;
		}
		levelIndex = nextLevelIndex;
	}

	ResetProgress();

	// Loads the progress file if it exists
	progressPath = appDataDirPath + "progress.txt";
	std::ifstream progressFileStream(progressPath, std::ios::binary);
	if (progressFileStream)
	{
		std::string line;
		while (std::getline(progressFileStream, line))
		{
			int64_t index = FindLevel(eg::TrimString(line));
			if (index != -1)
			{
				levels[index].status = LevelStatus::Completed;
				if (levels[index].nextLevelIndex != -1 &&
				    levels[levels[index].nextLevelIndex].status == LevelStatus::Locked)
				{
					levels[levels[index].nextLevelIndex].status = LevelStatus::Unlocked;
				}
			}
		}
	}
}

void ResetProgress()
{
	for (Level& level : levels)
	{
		if (!level.isExtra)
			level.status = LevelStatus::Locked;
	}
	levels[FindLevel(levelsOrder[0])].status = LevelStatus::Unlocked;
}

void SaveProgress()
{
	std::ofstream progressFileStream(progressPath, std::ios::binary);
	if (!progressFileStream)
	{
		eg::Log(eg::LogLevel::Error, "io", "Failed to save progress!");
		return;
	}
	WriteProgressToStream(progressFileStream);
	progressFileStream.close();
	SyncFileSystem();
}

void WriteProgressToStream(std::ostream& stream)
{
	for (const Level& level : levels)
	{
		if (level.status == LevelStatus::Completed)
		{
			stream << level.name << "\n";
		}
	}
}

int64_t FindLevel(std::string_view name)
{
	auto it = std::lower_bound(
		levels.begin(), levels.end(), name, [&](const Level& a, std::string_view b) { return a.name < b; }
	);

	if (it == levels.end() || it->name != name)
		return -1;
	return it - levels.begin();
}

std::string GetLevelPath(std::string_view name)
{
	return eg::Concat({ levelsDirPath, "/", name, ".gwd" });
}

std::tuple<std::unique_ptr<uint8_t, eg::FreeDel>, uint32_t, uint32_t> PlatformGetLevelThumbnailData(Level& level);

void LoadLevelThumbnail(Level& level)
{
	auto [data, width, height] = PlatformGetLevelThumbnailData(level);
	if (!data)
		return;

	level.thumbnail = eg::Texture::Create2D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample | eg::TextureFlags::GenerateMipmaps,
		.mipLevels = eg::Texture::MaxMipLevels(std::max(width, height)),
		.width = width,
		.height = height,
		.depth = 1,
		.format = eg::Format::R8G8B8A8_sRGB,
	});

	level.thumbnail.SetData(
		{ reinterpret_cast<char*>(data.get()), width * height * 4 },
		eg::TextureRange{
			.sizeX = width,
			.sizeY = height,
			.sizeZ = 1,
		}
	);

	eg::DC.GenerateMipmaps(level.thumbnail);

	level.thumbnail.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

size_t LevelsGraph::LevelConnectionKey::Hash() const
{
	size_t h = 0;
	eg::HashAppend(h, levelName);
	eg::HashAppend(h, gateName);
	return h;
}

std::optional<LevelsGraph> LevelsGraph::LoadFromFile()
{
	std::optional<eg::MemoryMappedFile> mappedFile = eg::MemoryMappedFile::OpenRead("Levels/graph.txt");
	if (!mappedFile.has_value())
		return std::nullopt;
	return Load(std::string_view(mappedFile->data.data(), mappedFile->data.size()));
}

LevelsGraph LevelsGraph::Load(std::string_view description)
{
	std::vector<std::string_view> lines;
	eg::SplitString(description, '\n', lines);

	LevelsGraph graph;

	for (size_t l = 0; l < lines.size(); l++)
	{
		std::string_view line = eg::TrimString(lines[l]);
		if (line.empty())
		{
			continue;
		}

		Node* prevNode = nullptr;

		eg::IterateStringParts(
			line, '-',
			[&](std::string_view lineNodeStr)
			{
				Node* node = nullptr;

				lineNodeStr = graph.m_allocator.MakeStringCopy(eg::TrimString(lineNodeStr));

				auto [beforeColon, afterColon] = eg::SplitStringOnce(lineNodeStr, ':');
				if (beforeColon.starts_with('@'))
				{
					std::string_view levelName = beforeColon.substr(1);
					if (FindLevel(levelName) == -1)
					{
						eg::Log(
							eg::LogLevel::Warning, "lvl", "LevelsGraph: On line {0}, level not found: {1}", l + 1,
							levelName
						);
					}
					else
					{
						node = graph.m_allocator.New<Node>(LevelConnectionNode{
							.levelName = levelName,
							.gateName = afterColon,
						});

						LevelConnectionKey key = { .levelName = levelName, .gateName = afterColon };
						if (!graph.m_levelConnectionNodeMap.emplace(key, node).second)
						{
							eg::Log(
								eg::LogLevel::Warning, "lvl", "LevelsGraph: Duplicate level&gate pair: {0}", lineNodeStr
							);
						}
					}
				}

				if (node != nullptr && prevNode != nullptr)
				{
					std::visit([&](auto& n) { n.neighbors.back() = node; }, *prevNode);
					std::visit([&](auto& n) { n.neighbors.front() = prevNode; }, *node);
				}
				prevNode = node;
			}
		);
	}

	return graph;
}

const LevelsGraph::Node* LevelsGraph::FindNodeByLevelConnection(std::string_view levelName, std::string_view gateName)
	const
{
	auto it = m_levelConnectionNodeMap.find(LevelConnectionKey{ .levelName = levelName, .gateName = gateName });
	if (it != m_levelConnectionNodeMap.end())
		return it->second;
	return nullptr;
}
