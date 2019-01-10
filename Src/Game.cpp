#include "Game.hpp"

#include "Graphics/Graphics.hpp"

void Game::Update()
{
	m_imGuiInterface.NewFrame();
	
	ImGui::ShowDemoWindow();
}

void Game::Draw()
{
	eg::DC.ClearColor(0, eg::Color(1, 1, 1));
	
	m_imGuiInterface.EndFrame(eg::CFrameIdx());
}
