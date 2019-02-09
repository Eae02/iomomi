#include "Game.hpp"
#include "GameState.hpp"
#include "MainGameState.hpp"
#include "Editor/Editor.hpp"
#include "Graphics/WallShader.hpp"
#include "Graphics/Materials/GravityCornerMaterial.hpp"

#include <fstream>

Game::Game()
{
	RenderSettings::instance = new RenderSettings;
	
	editor = new Editor(m_renderCtx);
	mainGameState = new MainGameState(m_renderCtx);
	currentGS = mainGameState;
	
	InitializeWallShader();
}

Game::~Game()
{
	delete editor;
	delete mainGameState;
	delete RenderSettings::instance;
}

void Game::ResolutionChanged(int newWidth, int newHeight)
{
	editor->SetResolution(newWidth, newHeight);
	mainGameState->SetResolution(newWidth, newHeight);
}

void Game::RunFrame(float dt)
{
	m_imGuiInterface.NewFrame();
	
	currentGS->RunFrame(dt);
	
	m_imGuiInterface.EndFrame();
	
	m_gameTime += dt;
}
