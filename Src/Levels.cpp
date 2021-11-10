#include "Levels.hpp"
#include "World/World.hpp"
#include "FileUtils.hpp"

#include <filesystem>
#include <fstream>

using namespace std::filesystem;

std::vector<Level> levels;

std::vector<std::string_view> levelsOrder = 
{
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
	"gravbarrier_ng3",
	
	"water_ng0",
	"water_ng1",
	"water_ng2",
	"water_ng3",
	"water_pump_ng1",
	"water_pump_ng2",
	"water_ng5",
	"water_pump_ng3",
	
	"gravbarrier_ng4",
	"gravbarrier_ng5",
	"gravbarrier_ng6",
	"gravbarrier_ng7",
	
	"gravgun_0",
	"gravgun_1",
	"gravgun_3",
	"gravgun_2",
	"gravgun_4",
	
	"forcefield_1",
	"forcefield_2",
	
	"gravbarrier_1",
	"gravbarrier_2",
	"gravbarrier_3",
	"gravbarrier_4",
	
	"gravgun_5",
	"gravgun_6",
	
	"forcefield_3",
	"forcefield_4",
	
	"water_0",
	"water_1",
	"water_2",
	"water_3",
	"water_4",
	"water_5",
	"water_6",
	"launch_0",
	"launch_1",
	"gravbarrier_cube_1",
	"gravbarrier_cube_2",
	"cubeflip_0",
	"launch_2",
	"gravbarrier_cube_3",
	"gravbarrier_cube_4",
	//"forcefield_5",
	"forcefield_6",
	
	"cubeflip_2"
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
	std::sort(levels.begin(), levels.end(), [&] (const Level& a, const Level& b)
	{
		return a.name < b.name;
	});
}

void InitLevels()
{
	levels.clear();
	InitLevelsPlatformDependent();
	
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
			levels[levelIndex].nextLevelIndex = nextLevelIndex;
			levels[levelIndex].isExtra = false;
		}
		levelIndex = nextLevelIndex;
	}
	
	ResetProgress();
	
	//Loads the progress file if it exists
	progressPath = appDataDirPath + "progress.txt";
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
	
	SyncFileSystem();
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
	return eg::Concat({ levelsDirPath, "/", name, ".gwd" });
}

std::tuple<std::unique_ptr<uint8_t, eg::FreeDel>, uint32_t, uint32_t> PlatformGetLevelThumbnailData(Level& level);

void LoadLevelThumbnail(Level& level)
{
	auto [data, width, height] = PlatformGetLevelThumbnailData(level);
	if (!data)
		return;
	
	eg::SamplerDescription samplerDesc;
	samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	
	eg::TextureCreateInfo textureCI;
	textureCI.width = width;
	textureCI.height = height;
	textureCI.depth = 1;
	textureCI.mipLevels = eg::Texture::MaxMipLevels(std::max(width, height));
	textureCI.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample | eg::TextureFlags::GenerateMipmaps;
	textureCI.format = eg::Format::R8G8B8A8_sRGB;
	textureCI.defaultSamplerDescription = &samplerDesc;
	
	level.thumbnail = eg::Texture::Create2D(textureCI);
	size_t uploadBufferSize = width * height * 4;
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBufferWith<uint8_t>({ data.get(), uploadBufferSize });
	
	eg::TextureRange texRange = {};
	
	texRange.sizeX = width;
	texRange.sizeY = height;
	texRange.sizeZ = 1;
	eg::DC.SetTextureData(level.thumbnail, texRange, uploadBuffer.buffer, uploadBuffer.offset);
	
	eg::DC.GenerateMipmaps(level.thumbnail);
	
	level.thumbnail.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}
