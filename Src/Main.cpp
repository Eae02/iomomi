#include "Game.hpp"
#include "Levels.hpp"

static_assert(sizeof(int) == 4);
static_assert(sizeof(float) == 4);

int main(int argc, char** argv)
{
	eg::RunConfig runConfig;
	runConfig.gameName = "Gravity";
	runConfig.flags = eg::RunFlags::DevMode | eg::RunFlags::DefaultFramebufferSRGB;
	runConfig.initialize = []
	{
		eg::LoadAssets("assets", "/");
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
}
