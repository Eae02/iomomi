#ifndef __EMSCRIPTEN__
#include <EGame/Graphics/ImageLoader.hpp>
#include <filesystem>
#include <fstream>
#include <memory>

#include "Levels.hpp"

extern std::string levelsDirPath;

static void UpgradeLevelsCommand(std::span<const std::string_view> args, eg::console::Writer& writer)
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

void InitLevelsPlatformDependent()
{
	eg::console::AddCommand("upgradeLevels", 0, &UpgradeLevelsCommand);

	levelsDirPath = eg::ExeRelPath("Levels");
	if (!eg::FileExists(levelsDirPath.c_str()))
	{
		levelsDirPath = "./Levels";
		if (!eg::FileExists(levelsDirPath.c_str()))
		{
			EG_PANIC("Missing directory \"Levels\".");
		}
	}

	std::string thumbnailsPath = levelsDirPath + "/img";
	eg::CreateDirectory(thumbnailsPath.c_str());

	// Adds all gwd files in the levels directory to the levels list
	for (const auto& entry : std::filesystem::directory_iterator(levelsDirPath))
	{
		if (is_regular_file(entry.status()) && entry.path().extension() == ".gwd")
		{
			Level& level = levels.emplace_back(entry.path().stem().string());
			LoadLevelThumbnail(level);
		}
	}

	SortLevels();
}

std::string GetLevelThumbnailPath(std::string_view name)
{
	return eg::Concat({ levelsDirPath, "/img/", name, ".jpg" });
}

std::tuple<std::unique_ptr<uint8_t, eg::FreeDel>, uint32_t, uint32_t> PlatformGetLevelThumbnailData(Level& level)
{
	std::string path = GetLevelThumbnailPath(level.name);
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return {};
	eg::ImageLoader loader(stream);
	return { loader.Load(4), loader.Width(), loader.Height() };
}

bool IsLevelLoadingComplete(std::string_view name)
{
	return true;
}

#endif
