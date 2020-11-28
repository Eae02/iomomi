#include "Game.hpp"
#include "Levels.hpp"
#include "Settings.hpp"
#include "Graphics/Materials/StaticPropMaterial.hpp"
#include "Graphics/Materials/DecalMaterial.hpp"

#include <google/protobuf/stubs/common.h>

#ifdef _WIN32
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

static_assert(sizeof(int) == 4);
static_assert(sizeof(float) == 4);

void Start(eg::RunConfig& runConfig)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	LoadSettings();
	eg::TextureAssetQuality = settings.textureQuality;
	
	runConfig.gameName = "Gravity Game";
	runConfig.flags |= eg::RunFlags::DefaultFramebufferSRGB;
	runConfig.defaultDepthStencilFormat = eg::Format::Depth32;
	runConfig.initialize = []
	{
		eg::SpriteFont::LoadDevFont();
		eg::console::Init();
		eg::console::AddCommand("opt", 1, &OptCommand);
		eg::console::SetCompletionProvider("opt", 0, &OptCommandCompleter1);
		eg::console::SetCompletionProvider("opt", 1, &OptCommandCompleter2);
		eg::console::AddCommand("optwnd", 0, &OptWndCommand);
		InitEntityTypes();
		StaticPropMaterial::InitAssetTypes();
		DecalMaterial::InitAssetTypes();
		if (!eg::LoadAssets("assets", "/"))
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
	eg::RunConfig runConfig;
	runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
	Start(runConfig);
}
#else
int main(int argc, char** argv)
{
	std::string appDataDirPath = eg::AppDataPath() + "/EaeGravity";
	if (!eg::FileExists(appDataDirPath.c_str()))
	{
		eg::CreateDirectory(appDataDirPath.c_str());
	}
	
	eg::RunConfig runConfig;
	runConfig.flags = eg::RunFlags::VSync;
#ifndef NDEBUG
	runConfig.flags |= eg::RunFlags::DevMode;
#endif
	
	eg::ParseCommandLineArgs(runConfig, argc, argv);
	
	Start(runConfig);
	
	SaveProgress();
	SaveSettings();
}
#endif
