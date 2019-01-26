#pragma once

#include "ImGuiInterface.hpp"
#include "Graphics/RenderContext.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"

class Game : public eg::IGame
{
public:
	Game();
	~Game();
	
	void RunFrame(float dt) override;
	
	void ResolutionChanged(int newWidth, int newHeight) override;
	
private:
	float m_gameTime = 0;
	
	ImGuiInterface m_imGuiInterface;
	
	RenderContext m_renderCtx;
};
