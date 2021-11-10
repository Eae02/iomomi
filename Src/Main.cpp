#include "Game.hpp"
#include "Levels.hpp"
#include "Settings.hpp"
#include "FileUtils.hpp"
#include "Graphics/Materials/StaticPropMaterial.hpp"
#include "Graphics/Materials/DecalMaterial.hpp"
#include "Graphics/GraphicsCommon.hpp"

#include <EGame/Audio/AudioPlayer.hpp>

#include <google/protobuf/stubs/common.h>

#ifdef _WIN32
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

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
		StaticPropMaterial::InitAssetTypes();
		DecalMaterial::InitAssetTypes();
		if (!eg::LoadAssets("Assets", "/"))
		{
			EG_PANIC("Failed to load assets, make sure assets.eap exists.");
		}
		RenderSettings::instance = new RenderSettings;
		InitLevels();
	};
	
	eg::Run<Game>(runConfig);
}

#ifdef __EMSCRIPTEN__
extern "C" void WebMain()
{
	appDataDirPath = "/data/";
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
