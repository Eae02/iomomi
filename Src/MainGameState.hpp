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
	
	void SetResolution(int width, int height);
	
	void LoadWorld(std::istream& stream);
	
private:
	void DrawOverlay(float dt);
	
	eg::PerspectiveProjection m_projection;
	RenderContext* m_renderCtx;
	
	float m_gameTime = 0;
	
	World m_world;
	Player m_player;
};

extern MainGameState* mainGameState;
