#pragma once

#include "ImGuiInterface.hpp"
#include "Graphics/RenderContext.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"

extern std::mt19937 globalRNG;

class Game : public eg::IGame
{
public:
	Game();
	~Game();
	
	void RunFrame(float dt) override;
	
private:
	float m_gameTime = 0;
	
	ImGuiInterface m_imGuiInterface;
	
	RenderContext m_renderCtx;
	
	uint64_t m_levelThumbnailUpdateFrameIdx = 0;
	struct LevelThumbnailUpdate* m_levelThumbnailUpdate = nullptr;
};
