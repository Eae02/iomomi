#include "Levels.hpp"
#include "World/World.hpp"

#ifdef __EMSCRIPTEN__
#include <experimental/filesystem>
using namespace std::experimental::filesystem;
#else
#include <filesystem>
#include <fstream>
#include <EGame/Graphics/ImageLoader.hpp>

using namespace std::filesystem;
#endif

std::vector<Level> levels;

const std::vector<std::string_view> levelsOrder = 
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
	//"water_ng4",
	"water_ng5",
	
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
	
	"gravgun_5",
	"gravgun_6",
	
	"forcefield_3",
	"forcefield_4",
	
	"water_0",
	"water_1",
	"gravbarrier_1",
	"gravbarrier_2",
	"gravbarrier_6",
	"water_2",
	"water_3",
	"water_4",
	"water_5",
	"water_6",
	"launch_0",
	"launch_3",
	"gravbarrier_3",
	"gravbarrier_4",
	"gravbarrier_5",
	"cubeflip_0",
	"launch_1",
	"launch_2",
	"forcefield_5",
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

std::unique_ptr<World> LoadLevelWorld(const Level& level, bool isEditor)
{
	std::string levelPath = GetLevelPath(level.name);
	std::ifstream levelStream(levelPath, std::ios::binary);
	return World::Load(levelStream, isEditor);
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
	
	std::string thumbnailsPath = levelsPath + "/img";
	eg::CreateDirectory(thumbnailsPath.c_str());
	
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
			levels[levelIndex].nextLevelIndex = nextLevelIndex;
			levels[levelIndex].isExtra = false;
		}
		levelIndex = nextLevelIndex;
	}
	
	ResetProgress();
	
	//Loads thumbnails
	for (Level& level : levels)
	{
		LoadLevelThumbnail(level);
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

std::string GetLevelThumbnailPath(std::string_view name)
{
	return eg::Concat({ eg::ExeDirPath(), "/levels/img/", name, ".jpg" });
}

void LoadLevelThumbnail(Level& level)
{
	std::string path = GetLevelThumbnailPath(level.name);
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return;
	eg::ImageLoader loader(stream);
	auto data = loader.Load(4);
	stream.close();
	if (!data)
		return;
	
	eg::SamplerDescription samplerDesc;
	samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	
	eg::TextureCreateInfo textureCI;
	textureCI.width = loader.Width();
	textureCI.height = loader.Height();
	textureCI.depth = 1;
	textureCI.mipLevels = eg::Texture::MaxMipLevels(std::max(loader.Width(), loader.Height()));
	textureCI.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample | eg::TextureFlags::GenerateMipmaps;
	textureCI.format = eg::Format::R8G8B8A8_sRGB;
	textureCI.defaultSamplerDescription = &samplerDesc;
	
	level.thumbnail = eg::Texture::Create2D(textureCI);
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBufferWith<uint8_t>({ data.get(), (size_t)(loader.Width() * loader.Height() * 4) });
	
	eg::TextureRange texRange = {};
	
	texRange.sizeX = loader.Width();
	texRange.sizeY = loader.Height();
	texRange.sizeZ = 1;
	eg::DC.SetTextureData(level.thumbnail, texRange, uploadBuffer.buffer, uploadBuffer.offset);
	
	eg::DC.GenerateMipmaps(level.thumbnail);
	
	level.thumbnail.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}
