#pragma once

#include <iosfwd>
#include <memory>
#include <functional>

struct DownloadedAssetBinary
{
	virtual ~DownloadedAssetBinary() { }
	virtual std::istream& GetStream() = 0;
};

#ifdef __EMSCRIPTEN__
using AssetDownloadCompletedCallback = std::function<void(std::unique_ptr<DownloadedAssetBinary>)>;
void BeginDownloadAssets(AssetDownloadCompletedCallback onComplete);
#endif
