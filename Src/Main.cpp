#include "Game.hpp"

static_assert(sizeof(int) == 4);
static_assert(sizeof(float) == 4);

int main(int argc, char** argv)
{
	eg::RunConfig runConfig;
	runConfig.gameName = "Gravity";
	runConfig.flags = eg::RunFlags::DevMode | eg::RunFlags::VSync;
	runConfig.initialize = []
	{
		eg::LoadAssets("assets", "/");
	};
	
	for (int i = 1; i < argc; i++)
	{
		std::string_view arg = argv[i];
		if (arg == "--gl")
			runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
		else if (arg == "--vk")
			runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
	}
	
	eg::Run<Game>(runConfig);
}
