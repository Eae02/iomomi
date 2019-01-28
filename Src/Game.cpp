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
	currentGS = editor;
	
	m_renderCtx.projection.SetFieldOfViewDeg(80.0f);
	
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
	m_renderCtx.projection.SetResolution(newWidth, newHeight);
}

void Game::RunFrame(float dt)
{
	m_imGuiInterface.NewFrame();
	
	currentGS->RunFrame(dt);
	
	m_imGuiInterface.EndFrame();
	
	m_gameTime += dt;
}
