#pragma once

class GameState
{
public:
	virtual void RunFrame(float dt) = 0;
};

extern GameState* currentGS;
