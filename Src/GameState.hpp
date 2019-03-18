#pragma once

class GameState
{
public:
	virtual ~GameState() = default;
	virtual void RunFrame(float dt) = 0;
};

extern GameState* currentGS;
