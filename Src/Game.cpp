#include "Game.hpp"
#include "GameState.hpp"
#include "MainGameState.hpp"
#include "Editor/Editor.hpp"
#include "Graphics/Materials/GravityCornerMaterial.hpp"

#include <fstream>

Game::Game()
{
	editor = new Editor(m_renderCtx);
	mainGameState = new MainGameState(m_renderCtx);
	currentGS = mainGameState;
	
	m_renderCtx.projection.SetFieldOfViewDeg(80.0f);
}

Game::~Game()
{
	delete editor;
	delete mainGameState;
}

void Game::ResolutionChanged(int newWidth, int newHeight)
{
	m_renderCtx.projection.SetResolution(newWidth, newHeight);
}

void Game::RunFrame(float dt)
{
	m_imGuiInterface.NewFrame();
	
	currentGS->RunFrame(dt);
	
	m_imGuiInterface.EndFrame();
	
	m_gameTime += dt;
}
