#pragma once

struct GameState
{
	virtual ~GameState() = default;
	virtual void RunFrame(float dt) = 0;
	virtual void OnDeactivate() {}
};

GameState* CurrentGS();

void SetCurrentGS(GameState* gs);
