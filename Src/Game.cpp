#include "Game.hpp"
#include "GameState.hpp"
#include "MainGameState.hpp"
#include "Levels.hpp"
#include "Editor/Editor.hpp"
#include "Settings.hpp"
#include "Graphics/WallShader.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"

#include <fstream>

Game::Game()
{
	editor = new Editor(m_renderCtx);
	mainGameState = new MainGameState(m_renderCtx);
	brdfIntegrationMap = new eg::BRDFIntegrationMap();
	
#ifndef NDEBUG
	SetCurrentGS(editor);
#endif
	
	eg::console::AddCommand("ed", 0, [&] (eg::Span<const std::string_view> args)
	{
		SetCurrentGS(editor);
		eg::console::Hide();
	});
	
	eg::console::AddCommand("play", 1, [this] (eg::Span<const std::string_view> args)
	{
		int64_t levelIndex = FindLevel(args[0]);
		if (levelIndex == -1)
		{
			eg::Log(eg::LogLevel::Error, "lvl", "Level not found: {0}", args[0]);
			return;
		}
		
		std::string levelPath = GetLevelPath(levels[levelIndex].name);
		std::ifstream levelStream(levelPath, std::ios::binary);
		
		SetCurrentGS(mainGameState);
		mainGameState->LoadWorld(levelStream, levelIndex);
		
		eg::console::Hide();
	});
	
	eg::console::SetCompletionProvider("play", 0, [] (eg::Span<const std::string_view> args, eg::console::CompletionsList& list)
	{
		for (const Level& level : levels)
			list.Add(level.name);
	});
	
	InitializeWallShader();
}

Game::~Game()
{
	delete editor;
	delete mainGameState;
	delete RenderSettings::instance;
	delete brdfIntegrationMap;
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
	
	DrawSettingsWindow();
	
	if (CurrentGS() != nullptr)
	{
		CurrentGS()->RunFrame(dt);
	}
	else
	{
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
		rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::ColorSRGB(0.1f, 0.1f, 0.2f));
		eg::DC.BeginRenderPass(rpBeginInfo);
		eg::DC.EndRenderPass();
	}
	
	m_imGuiInterface.EndFrame();
	
	m_gameTime += dt;
}
