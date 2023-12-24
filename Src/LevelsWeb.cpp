#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>

#include <EGame/Graphics/ImageLoader.hpp>
#include <cstring>
#include <istream>
#include <memory>

#include "Levels.hpp"

struct LevelData
{
	std::vector<char> gwdData;
	std::vector<char> thumbnailData;
	bool thumbnailLoadingComplete = false;
	std::string_view levelName;

	explicit LevelData(std::string_view _levelName) : levelName(_levelName) {}
};

static std::unordered_map<std::string_view, LevelData> levelData;

void BeginDownloadThumbnail(std::string_view levelName)
{
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = [](emscripten_fetch_t* fetch)
	{
		LevelData& data = *reinterpret_cast<LevelData*>(fetch->userData);
		data.thumbnailData.resize(fetch->numBytes);
		std::memcpy(data.thumbnailData.data(), fetch->data, fetch->numBytes);
		data.thumbnailLoadingComplete = true;
		LoadLevelThumbnail(levels[FindLevel(data.levelName)]);
		emscripten_fetch_close(fetch);
	};
	attr.onerror = [](emscripten_fetch_t* fetch)
	{
		eg::Log(
			eg::LogLevel::Error, "lvl", "Failed to download level thumbnail from {0}: {1}", fetch->url, fetch->status);
		reinterpret_cast<LevelData*>(fetch->userData)->thumbnailLoadingComplete = true;
		emscripten_fetch_close(fetch);
	};
	attr.userData = &levelData.at(levelName);
	std::string url = eg::Concat({ "Levels/img/", levelName, ".jpg" });
	emscripten_fetch(&attr, url.c_str());
}

void BeginDownloadGWD(std::string_view levelName)
{
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = [](emscripten_fetch_t* fetch)
	{
		LevelData& data = *reinterpret_cast<LevelData*>(fetch->userData);
		data.gwdData.resize(fetch->numBytes);
		std::memcpy(data.gwdData.data(), fetch->data, fetch->numBytes);
		emscripten_fetch_close(fetch);
	};
	attr.onerror = [](emscripten_fetch_t* fetch)
	{
		eg::Log(eg::LogLevel::Error, "lvl", "Failed to download level gwd from {0}: {1}", fetch->url, fetch->status);
		emscripten_fetch_close(fetch);
	};
	attr.userData = &levelData.at(levelName);
	std::string url = eg::Concat({ "Levels/", levelName, ".gwd" });
	emscripten_fetch(&attr, url.c_str());
}

void GetProgressCommand(std::span<const std::string_view> args, eg::console::Writer& writer)
{
	std::ostringstream stream;
	WriteProgressToStream(stream);
	std::string progress = stream.str();

	EM_ASM(
		{
			const blob = new Blob(
				[UTF8ToString($0)],
				{
					type:
						'text/plain'
				});
			const link = window.URL.createObjectURL(blob);
			if (link)
			{
				const a = document.createElement("a");
				a.style.display = "none";
				a.href = link;
				a.download = "progress.txt";
				document.body.appendChild(a);
				a.click();
				setTimeout(
					() = >
			             {
							 URL.revokeObjectURL(link);
							 link.parentNode.removeChild(a);
						 },
					0);
			}
		},
		progress.c_str());
}

void InitLevelsPlatformDependent()
{
	eg::console::AddCommand("getProgress", 0, &GetProgressCommand);

	for (int64_t i = eg::ToInt64(levelsOrder.size()) - 1; i >= 0; i--)
	{
		levelData.emplace(levelsOrder[i], LevelData(levelsOrder[i]));
		levels.emplace_back(std::string(levelsOrder[i]));
	}

	SortLevels();

	for (std::string_view levelName : levelsOrder)
	{
		BeginDownloadGWD(levelName);
		BeginDownloadThumbnail(levelName);
	}
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
		return {};
	eg::MemoryStreambuf streambuf(it->second.thumbnailData);
	std::istream thumbnailStream(&streambuf);
	eg::ImageLoader loader(thumbnailStream);
	return { loader.Load(4), loader.Width(), loader.Height() };
}

bool IsLevelLoadingComplete(std::string_view name)
{
	auto it = levelData.find(name);
	return it != levelData.end() && !it->second.gwdData.empty() && it->second.thumbnailLoadingComplete;
}

#endif
