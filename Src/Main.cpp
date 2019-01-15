#include "Game.hpp"

int main()
{
	eg::RunConfig runConfig;
	runConfig.gameName = "Gravity";
	runConfig.initialize = []
	{
		eg::LoadAssets("assets", "/");
	};
	
	eg::Run<Game>(runConfig);
}
