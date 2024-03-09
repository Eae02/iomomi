#pragma once

#include <pcg_random.hpp>

#include "Graphics/RenderContext.hpp"
#include "World/Player.hpp"
#include "World/World.hpp"

extern pcg32_fast globalRNG;

class Game : public eg::IGame
{
public:
	Game();
	~Game();

	void RunFrame(float dt) override;

	static int64_t levelIndexFromCmdArg;

private:
	float m_gameTime = 0;

	RenderContext m_renderCtx;

	class GameState* m_editorGameState;

	uint64_t m_levelThumbnailUpdateFrameIdx = 0;
	struct LevelThumbnailUpdate* m_levelThumbnailUpdate = nullptr;
};
