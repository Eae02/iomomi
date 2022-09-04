#ifdef __EMSCRIPTEN__
#include "WebAssetDownload.hpp"
#include "FileUtils.hpp"

#ifndef BUILD_ID
#error BUILD_ID must be defined when building for emscripten
#endif

#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>

static const char* cachedAssetsFilePath = "/data/cached_assets.eap";
static const char* cachedAssetsIdFilePath = "/data/cached_assets_id.txt";

struct FetchedAssetBinary : DownloadedAssetBinary
{
	emscripten_fetch_t* fetch;
	eg::MemoryStreambuf streambuf;
	std::istream stream;
	
	explicit FetchedAssetBinary(emscripten_fetch_t* _fetch)
		: fetch(_fetch), streambuf(fetch->data, fetch->data + fetch->numBytes), stream(&streambuf) { }
	
	~FetchedAssetBinary()
	{
		emscripten_fetch_close(fetch);
	}
	
	std::istream& GetStream() override { return stream; }
};

struct CachedAssetBinary : DownloadedAssetBinary
{
	std::ifstream stream;
	CachedAssetBinary(std::ifstream _stream) : stream(std::move(_stream)) { }
	std::istream& GetStream() override { return stream; }
};

static AssetDownloadCompletedCallback onAssetDownloadComplete;

static void WriteAssetsToCache(std::span<const char> data)
{
	std::ofstream cachedAssetsStream(cachedAssetsFilePath, std::ios::binary);
	if (!cachedAssetsStream)
	{
		std::cout << "[assetcache] Failed to open " << cachedAssetsFilePath << " for writing" << std::endl;
		return;
	}
	cachedAssetsStream.write(data.data(), data.size());
	cachedAssetsStream.close();
	
	std::ofstream cachedAssetsIdStream(cachedAssetsIdFilePath);
	if (!cachedAssetsIdStream)
	{
		std::cout << "[assetcache] Failed to open " << cachedAssetsIdFilePath << " for writing" << std::endl;
		return;
	}
	
	cachedAssetsIdStream.write(BUILD_ID, strlen(BUILD_ID));
	cachedAssetsIdStream.close();
	
	SyncFileSystem();
}

static void AssetDownloadCompleted(emscripten_fetch_t* fetch)
{
	WriteAssetsToCache({ fetch->data, static_cast<size_t>(fetch->numBytes) });
	
	emscripten_async_call([] (void* userdata)
	{
		onAssetDownloadComplete(std::make_unique<FetchedAssetBinary>(static_cast<emscripten_fetch_t*>(userdata)));
	}, fetch, 0);
}

static void AssetDownloadFailed(emscripten_fetch_t* fetch)
{
	EM_ASM({
		setInfoLabel('Got response: ' + $0);
		displayError('Failed to download assets');
	}, fetch->status);
	emscripten_fetch_close(fetch);
}

static void AssetDownloadProgress(emscripten_fetch_t* fetch)
{
	std::ostringstream messageStream;
	messageStream << "Downloading assets... (";
	
	if (fetch->totalBytes)
	{
		messageStream
			<< std::setprecision(1) << std::fixed << ((double)fetch->dataOffset / 1048576.0) << " / "
			<< std::setprecision(1) << std::fixed << ((double)fetch->totalBytes / 1048576.0);
	}
	else
	{
		messageStream << std::setprecision(1) << std::fixed << (static_cast<double>(fetch->dataOffset + fetch->numBytes) / 1048576.0);
	}
	messageStream << " MiB)";
	
	std::string message = messageStream.str();
	EM_ASM({
		setInfoLabel(UTF8ToString($0));
	}, message.c_str());
}

static std::unique_ptr<CachedAssetBinary> TryLoadCachedAssets()
{
	std::ifstream cachedAssetsIdStream(cachedAssetsIdFilePath);
	if (!cachedAssetsIdStream)
	{
		std::cout << "[assetcache] Failed to open " << cachedAssetsIdFilePath << std::endl;
		return nullptr;
	}
	
	std::string cachedId;
	std::getline(cachedAssetsIdStream, cachedId);
	if (eg::TrimString(cachedId) != BUILD_ID)
	{
		std::cout << "[assetcache] Version mismatch (got " << cachedId << " expected " BUILD_ID ")" << std::endl;
		return nullptr;
	}
	cachedAssetsIdStream.close();
	
	std::ifstream cachedAssetsStream(cachedAssetsFilePath, std::ios::binary);
	if (!cachedAssetsStream)
	{
		std::cout << "[assetcache] Failed to open " << cachedAssetsFilePath << std::endl;
		return nullptr;
	}
	
	static const char EAP_MAGIC[4] = { -1, 'E', 'A', 'P' };
	char magic[4];
	cachedAssetsStream.read(magic, 4);
	if (std::memcmp(magic, EAP_MAGIC, 4) != 0)
	{
		std::cout << "[assetcache] Package corrupted " << std::hex << *reinterpret_cast<uint32_t*>(magic) << std::dec << std::endl;
		return nullptr;
	}
	
	cachedAssetsStream.seekg(std::ios::beg);
	return std::make_unique<CachedAssetBinary>(std::move(cachedAssetsStream));
}

void BeginDownloadAssets(AssetDownloadCompletedCallback onComplete)
{
	if (std::unique_ptr<CachedAssetBinary> cachedAssetBinary = TryLoadCachedAssets())
	{
		std::cout << "[assetcache] Cache valid, loading assets from cache" << std::endl;
		onComplete(std::move(cachedAssetBinary));
	}
	else
	{
		std::cout << "[assetcache] Cache invalid, downloading assets" << std::endl;
		onAssetDownloadComplete = std::move(onComplete);
		
		emscripten_fetch_attr_t attr;
		emscripten_fetch_attr_init(&attr);
		strcpy(attr.requestMethod, "GET");
		attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
		attr.onsuccess = AssetDownloadCompleted;
		attr.onerror = AssetDownloadFailed;
		attr.onprogress = AssetDownloadProgress;
		emscripten_fetch(&attr, "Assets.eap");
	}
}

#endif
