#include "Game.hpp"
#include "Levels.hpp"
#include "Settings.hpp"
#include "FileUtils.hpp"
#include "WebAssetDownload.hpp"
#include "Graphics/GraphicsCommon.hpp"

#include <EGame/Audio/AudioPlayer.hpp>

#include <google/protobuf/stubs/common.h>

#ifdef _WIN32
extern "C"
{
	__declspec(dllexport) uint32_t NvOptimusEnablement = 1;
	__declspec(dllexport) uint32_t AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static_assert(sizeof(int) == 4);
static_assert(sizeof(float) == 4);

const char* version = __DATE__;

bool audioInitializationFailed = false;

void InitializeStaticPropMaterialAsset();
void InitializeDecalMaterialAsset();

static void Run(int argc, char** argv, std::unique_ptr<DownloadedAssetBinary> downloadedAssetBinary)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	eg::RunConfig runConfig;
#ifndef NDEBUG
	runConfig.flags |= eg::RunFlags::DevMode;
#endif
	
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
	runConfig.initialize = [&]
	{
		AssertRenderTextureFormatSupport();
		
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
		if (downloadedAssetBinary)
		{
			assetLoadOK = eg::LoadAssetsFromEAPStream(downloadedAssetBinary->GetStream(), "/");
			downloadedAssetBinary.reset();
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
extern "C" void WebMain()
{
	eg::releasePanicCallback = [] (const std::string& message)
	{
		EM_ASM({ setInfoLabel(UTF8ToString($0)); }, message.c_str());
	};
	
	appDataDirPath = "/data/";
	
	BeginDownloadAssets([] (std::unique_ptr<DownloadedAssetBinary> downloadedAssetBinary)
	{
		EM_ASM( setInfoLabel(""); );
		char argv[] = "iomomi";
		char* argvPtr = argv;
		Run(1, &argvPtr, std::move(downloadedAssetBinary));
	});
}
#else
int main(int argc, char** argv)
{
	appDataDirPath = eg::AppDataPath() + "/iomomi/";
	if (!eg::FileExists(appDataDirPath.c_str()))
	{
		eg::CreateDirectory(appDataDirPath.c_str());
	}
	
	Run(argc, argv, nullptr);
	SaveProgress();
	SaveSettings();
}
#endif
