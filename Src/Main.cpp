#include "Game.hpp"
#include "Levels.hpp"
#include "Settings.hpp"
#include "FileUtils.hpp"
#include "Graphics/GraphicsCommon.hpp"

#include <EGame/Audio/AudioPlayer.hpp>

#include <google/protobuf/stubs/common.h>

#ifdef __EMSCRIPTEN__
#include <sstream>
#include <iomanip>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#endif

#ifdef _WIN32
extern "C"
{
	__declspec(dllexport) uint32_t NvOptimusEnablement = 1;
	__declspec(dllexport) uint32_t AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static_assert(sizeof(int) == 4);
static_assert(sizeof(float) == 4);

const char* version = __DATE__;

bool audioInitializationFailed = false;

void InitializeStaticPropMaterialAsset();
void InitializeDecalMaterialAsset();

std::vector<char> preloadedAssetBinary;

void Run(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	eg::RunConfig runConfig;
#ifndef NDEBUG
	runConfig.flags |= eg::RunFlags::DevMode;
#endif
	
	InitEntityTypes();
	LoadSettings();
	eg::TextureAssetQuality = settings.textureQuality;
	
	if (settings.vsync)
		runConfig.flags |= eg::RunFlags::VSync;
	runConfig.graphicsAPI = settings.graphicsAPI;
	runConfig.preferredGPUName = settings.preferredGPUName;
	
	eg::ParseCommandLineArgs(runConfig, argc, argv);
	if (eg::HasFlag(runConfig.flags, eg::RunFlags::PreferIntegratedGPU))
		runConfig.preferredGPUName.clear();
#ifndef __EMSCRIPTEN__
	useGLESPath = eg::HasFlag(runConfig.flags, eg::RunFlags::PreferGLESPath);
#endif
	
	runConfig.gameName = "Iomomi";
	runConfig.flags |= eg::RunFlags::DefaultFramebufferSRGB;
	runConfig.defaultDepthStencilFormat = eg::Format::Depth32;
	runConfig.framerateCap = 200;
	runConfig.minWindowW = 1000;
	runConfig.minWindowH = 700;
	runConfig.initialize = []
	{
		if (!eg::FullscreenDisplayModes().empty())
		{
			if (!eg::Contains(eg::FullscreenDisplayModes(), settings.fullscreenDisplayMode))
			{
				settings.fullscreenDisplayMode = eg::FullscreenDisplayModes()[eg::NativeDisplayModeIndex()];
			}
			UpdateDisplayMode();
		}
		
		if (!eg::InitializeAudio())
		{
			eg::Log(eg::LogLevel::Error, "audio", "Failed to initialize audio, sounds will not be played.");
			audioInitializationFailed = true;
		}
		UpdateVolumeSettings();
		
		eg::SpriteFont::LoadDevFont();
		eg::console::Init();
		InitializeStaticPropMaterialAsset();
		InitializeDecalMaterialAsset();
		
		RenderSettings::instance = new RenderSettings;
		
		bool assetLoadOK = false;
		if (!preloadedAssetBinary.empty())
		{
			eg::MemoryStreambuf streambuf(preloadedAssetBinary);
			std::istream eapStream(&streambuf);
			assetLoadOK = eg::LoadAssetsFromEAPStream(eapStream, "/");
		}
		else
		{
			assetLoadOK = eg::LoadAssets("Assets", "/");
		}
		if (!assetLoadOK)
		{
			EG_PANIC("Failed to load assets, make sure assets.eap exists.");
		}
		
		InitLevels();
		
#ifdef __EMSCRIPTEN__
		EM_ASM( loadingComplete(); );
#endif
	};
	
	eg::Run<Game>(runConfig);
}

#ifdef __EMSCRIPTEN__
void AssetDownloadCompleted(emscripten_fetch_t* fetch)
{
	EM_ASM( setInfoLabel(""); );
	
	preloadedAssetBinary.resize(fetch->numBytes);
	std::memcpy(preloadedAssetBinary.data(), fetch->data, fetch->numBytes);
	emscripten_fetch_close(fetch);
	
	emscripten_async_call([] (void*)
	{
		appDataDirPath = "/data/";
		char argv[] = "iomomi";
		char* argvPtr = argv;
		Run(1, &argvPtr);
	}, nullptr, 0);
}

void AssetDownloadFailed(emscripten_fetch_t* fetch)
{
	EM_ASM({
		setInfoLabel('Got response: ' + $0);
		displayError('Failed to download assets');
	}, fetch->status);
	emscripten_fetch_close(fetch);
}

void AssetDownloadProgress(emscripten_fetch_t* fetch)
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
		messageStream << std::setprecision(1) << std::fixed << ((double)(fetch->dataOffset + fetch->numBytes) / 1048576.0);
	}
	messageStream << " MiB)";
	
	std::string message = messageStream.str();
	EM_ASM({
		setInfoLabel(UTF8ToString($0));
	}, message.c_str());
}

extern "C" void WebMain()
{
	eg::releasePanicCallback = [] (const std::string& message)
	{
		EM_ASM({
			setInfoLabel(UTF8ToString($0));
		}, message.c_str());
	};
	
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = AssetDownloadCompleted;
	attr.onerror = AssetDownloadFailed;
	attr.onprogress = AssetDownloadProgress;
	emscripten_fetch(&attr, "Assets.eap");
}
#else
int main(int argc, char** argv)
{
	appDataDirPath = eg::AppDataPath() + "/iomomi/";
	if (!eg::FileExists(appDataDirPath.c_str()))
	{
		eg::CreateDirectory(appDataDirPath.c_str());
	}
	
	Run(argc, argv);
	SaveProgress();
	SaveSettings();
}
#endif
