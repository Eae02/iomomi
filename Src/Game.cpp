#include "Game.hpp"
#include "GameState.hpp"
#include "MainGameState.hpp"
#include "Editor/Editor.hpp"
#include "Graphics/WallShader.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"

#include <fstream>

Game::Game()
{
	editor = new Editor(m_renderCtx);
	mainGameState = new MainGameState(m_renderCtx);
	currentGS = editor;
	
	eg::console::AddCommand("ed", 0, [&] (eg::Span<const std::string_view> args)
	{
		currentGS = editor;
		eg::console::Hide();
	});
	
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
	
	m_renderCtx.renderer.PollSettingsChanged();
	
	currentGS->RunFrame(dt);
	
	m_imGuiInterface.EndFrame();
	
	m_gameTime += dt;
}
