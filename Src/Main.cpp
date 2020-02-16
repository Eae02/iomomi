#include "Game.hpp"
#include "Levels.hpp"
#include "Settings.hpp"
#include "World/BulletPhysics.hpp"
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
	
	bullet::Init();
	
	LoadSettings();
	eg::TextureAssetQuality = settings.textureQuality;
	
	runConfig.gameName = "Gravity Game";
	runConfig.flags |= eg::RunFlags::DevMode | eg::RunFlags::DefaultFramebufferSRGB;
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
		eg::LoadAssets("assets", "/");
		RenderSettings::instance = new RenderSettings;
		InitLevels();
		InitLevelsGraph();
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
	bool vSync = true;
	for (int i = 1; i < argc; i++)
	{
		std::string_view arg = argv[i];
		if (arg == "--gl")
			runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
		else if (arg == "--vk")
			runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
		else if (arg == "--novsync")
			vSync = false;
	}
	if (vSync)
	{
		runConfig.flags |= eg::RunFlags::VSync;
	}
	Start(runConfig);
	
	bullet::Destroy();
	
	SaveSettings();
}
#endif
