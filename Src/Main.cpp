#include <google/protobuf/stubs/common.h>

#include <EGame/Audio/AudioPlayer.hpp>

#include "FileUtils.hpp"
#include "Game.hpp"
#include "Graphics/GraphicsCommon.hpp"
#include "Levels.hpp"
#include "Settings.hpp"

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

void AssetDownloadProgress(const eg::DownloadProgress& progress)
{
#ifdef __EMSCRIPTEN__
	std::string message = progress.CreateMessage();
	EM_ASM({ setInfoLabel(UTF8ToString($0)); }, message.c_str());
#endif
}

static void Run(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	eg::DownloadAssetPackageASync(eg::DownloadAssetPackageArgs{
		.eapName = "Assets.eap",
#ifdef BUILD_ID
		.cacheID = BUILD_ID,
#endif
		.freeAfterInit = true,
		.progressCallback = AssetDownloadProgress,
	});

	eg::RunConfig runConfig;
#ifndef NDEBUG
	runConfig.flags |= eg::RunFlags::DevMode;
#endif

	LoadSettings();
	eg::TextureAssetQuality = settings.textureQuality;
	SetLightColorAttachmentFormat(settings.highDynamicRange);

	if (settings.vsync)
		runConfig.flags |= eg::RunFlags::VSync;
	runConfig.preferredGPUName = settings.preferredGPUName;

#ifndef __APPLE__
	runConfig.graphicsAPI = settings.graphicsAPI;
#endif

	eg::ParseCommandLineArgs(runConfig, argc, argv);
	if (eg::HasFlag(runConfig.flags, eg::RunFlags::PreferIntegratedGPU))
		runConfig.preferredGPUName.clear();

#if defined(__EMSCRIPTEN__) || defined(__APPLE__)
	useGLESPath = true;
#else
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
		// AssertRenderTextureFormatSupport();

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

		GraphicsCommonInit();
		eg::SpriteFont::LoadDevFont();
		eg::console::Init();
		InitializeStaticPropMaterialAsset();
		InitializeDecalMaterialAsset();

		RenderSettings::instance = new RenderSettings;

		if (!eg::LoadAssets("Assets", "/"))
		{
			EG_PANIC("Failed to load assets, make sure assets.eap exists.");
		}

		InitLevels();

#ifdef __EMSCRIPTEN__
		EM_ASM(loadingComplete(););
#endif
	};

	eg::Run<Game>(runConfig);
}

#ifdef __EMSCRIPTEN__
extern "C" void EMSCRIPTEN_KEEPALIVE WebMain()
{
	eg::releasePanicCallback = [](const std::string& message)
	{ EM_ASM({ setInfoLabel(UTF8ToString($0)); }, message.c_str()); };

	appDataDirPath = "/data/";

	EM_ASM(setInfoLabel(""););
	char argv[] = "iomomi";
	char* argvPtr = argv;
	Run(1, &argvPtr);
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
