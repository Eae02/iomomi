#include "GameState.hpp"

static GameState* currentGS = nullptr;

GameState* CurrentGS()
{
	return currentGS;
}

void SetCurrentGS(GameState* gs)
{
	if (currentGS != nullptr)
		currentGS->OnDeactivate();
	currentGS = gs;
}
