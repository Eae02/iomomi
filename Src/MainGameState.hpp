#pragma once

#include "GameState.hpp"
#include "Graphics/RenderContext.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"

class MainGameState : public GameState
{
public:
	explicit MainGameState(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
private:
	void DrawOverlay(float dt);
	
	RenderContext* m_renderCtx;
	
	World m_world;
	Player m_player;
};

extern MainGameState* mainGameState;
