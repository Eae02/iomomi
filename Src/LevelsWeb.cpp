#ifdef __EMSCRIPTEN__
#include "Levels.hpp"

#include <unzip.h>

#include <istream>
#include <memory>
#include <EGame/Graphics/ImageLoader.hpp>

struct LevelData
{
	std::vector<char> gwdData;
	std::vector<char> thumbnailData;
};

static std::unordered_map<std::string, LevelData> levelData;

void InitLevelsPlatformDependent()
{
	unzFile zipfile = unzOpen("Levels.zip");
	if (zipfile == nullptr)
	{
		EG_PANIC("Error opening Levels.zip")
	}
	
	unz_global_info globalInfo;
	if (unzGetGlobalInfo(zipfile, &globalInfo) != UNZ_OK )
	{
		EG_PANIC("Error reading Levels.zip")
	}
	
	unzGoToFirstFile(zipfile);
	
	char fileNameBuffer[256];
	
	for (uLong i = 0; i < globalInfo.number_entry; i++)
	{
		unz_file_info fileInfo;
		if (unzGetCurrentFileInfo(zipfile, &fileInfo, fileNameBuffer, sizeof(fileNameBuffer), nullptr, 0, nullptr, 0) != UNZ_OK)
		{
			EG_PANIC("Error reading file info from Levels.zip")
		}
		
		std::string_view fileName = fileNameBuffer;
		
		if (fileName.ends_with(".gwd") || (fileName.starts_with("Levels/img/") && fileName.ends_with(".jpg")))
		{
			std::string levelName(eg::PathWithoutExtension(eg::BaseName(fileName)));
			
			LevelData* data;
			auto dataIt = levelData.find(levelName);
			if (dataIt != levelData.end())
			{
				data = &dataIt->second;
			}
			else
			{
				data = &levelData.emplace(levelName, LevelData()).first->second;
			}
			
			unzOpenCurrentFile(zipfile);
			
			std::vector<char> fileData(fileInfo.uncompressed_size);
			if (unzReadCurrentFile(zipfile, fileData.data(), fileData.size()) < 0)
			{
				eg::Log(eg::LogLevel::Error, "lvl", "Error reading level file '{0}'", fileName);
			}
			else if (fileName.ends_with(".gwd"))
			{
				levels.push_back({levelName});
				data->gwdData = std::move(fileData);
			}
			else if (fileName.ends_with(".jpg"))
			{
				data->thumbnailData = std::move(fileData);
			}
			
			unzCloseCurrentFile(zipfile);
		}
		else if (!fileName.ends_with("/"))
		{
			eg::Log(eg::LogLevel::Info, "lvl", "Unexpected level file in zip {0}", fileName);
		}
		
		if (i != globalInfo.number_entry - 1)
			unzGoToNextFile(zipfile);
	}
	
	unzClose(zipfile);
}

std::unique_ptr<World> LoadLevelWorld(const Level& level, bool isEditor)
{
	auto it = levelData.find(level.name);
	if (it == levelData.end() || it->second.gwdData.empty())
		return nullptr;
	eg::MemoryStreambuf streambuf(it->second.gwdData);
	std::istream levelStream(&streambuf);
	return World::Load(levelStream, isEditor);
}

std::tuple<std::unique_ptr<uint8_t, eg::FreeDel>, uint32_t, uint32_t> PlatformGetLevelThumbnailData(Level& level)
{
	auto it = levelData.find(level.name);
	if (it == levelData.end() || it->second.thumbnailData.empty())
		return { };
	eg::MemoryStreambuf streambuf(it->second.thumbnailData);
	std::istream thumbnailStream(&streambuf);
	eg::ImageLoader loader(thumbnailStream);
	return { loader.Load(4), loader.Width(), loader.Height() };
}

#endif
