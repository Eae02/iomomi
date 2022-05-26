#pragma once

#include "Graphics/RenderContext.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"

#include <pcg_random.hpp>

extern pcg32_fast globalRNG;

class Game : public eg::IGame
{
public:
	Game();
	~Game();
	
	void RunFrame(float dt) override;
	
private:
	float m_gameTime = 0;
	
	RenderContext m_renderCtx;
	
	class GameState* m_editorGameState;
	
	uint64_t m_levelThumbnailUpdateFrameIdx = 0;
	struct LevelThumbnailUpdate* m_levelThumbnailUpdate = nullptr;
};
