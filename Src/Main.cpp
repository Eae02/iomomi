#include "Game.hpp"
#include "Levels.hpp"
#include "Settings.hpp"
#include "World/BulletPhysics.hpp"
#include "Graphics/Materials/StaticPropMaterial.hpp"
#include "Graphics/Materials/DecalMaterial.hpp"

#include <google/protobuf/stubs/common.h>

static_assert(sizeof(int) == 4);
static_assert(sizeof(float) == 4);

void InitEntitySerializers();

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	bullet::Init();
	
	std::string appDataDirPath = eg::AppDataPath() + "EaeGravity";
	if (!eg::FileExists(appDataDirPath.c_str()))
	{
		eg::CreateDirectory(appDataDirPath.c_str());
	}
	
	LoadSettings();
	eg::TextureAssetQuality = settings.textureQuality;
	
	eg::RunConfig runConfig;
	runConfig.gameName = "Gravity";
	runConfig.flags = eg::RunFlags::DevMode | eg::RunFlags::DefaultFramebufferSRGB;
	runConfig.defaultDepthStencilFormat = eg::Format::Depth32;
	runConfig.initialize = []
	{
		eg::console::AddCommand("opt", 1, &OptCommand);
		InitEntitySerializers();
		StaticPropMaterial::InitAssetTypes();
		DecalMaterial::InitAssetTypes();
		eg::LoadAssets("assets", "/");
		RenderSettings::instance = new RenderSettings;
		InitLevels();
	};
	
	bool vSync = true;
	for (int i = 1; i < argc; i++)
	{
		std::string_view arg = argv[i];
		if (arg == "--gl")
			runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
		else if (arg == "--vk")
			runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
		else if (arg == "--no-vsync")
			vSync = false;
	}
	
	if (vSync)
		runConfig.flags |= eg::RunFlags::VSync;
	
	eg::Run<Game>(runConfig);
	
	bullet::Destroy();
}
