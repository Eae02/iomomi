#include "Game.hpp"
#include "GameState.hpp"
#include "MainGameState.hpp"
#include "Levels.hpp"
#include "Editor/Editor.hpp"
#include "Settings.hpp"
#include "Graphics/GraphicsCommon.hpp"
#include "Graphics/WallShader.hpp"
#include "MainMenuGameState.hpp"
#include "ThumbnailRenderer.hpp"

#include <fstream>

Game::Game()
{
	editor = new Editor(m_renderCtx);
	mainGameState = new MainGameState(m_renderCtx);
	mainMenuGameState = new MainMenuGameState();
	
	SetCurrentGS(mainMenuGameState);
	
	eg::console::AddCommand("ed", 0, [&] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		SetCurrentGS(editor);
		eg::console::Hide();
	});
	
	eg::console::AddCommand("mm", 0, [&] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		mainMenuGameState->GoToMainScreen();
		SetCurrentGS(mainMenuGameState);
		eg::console::Hide();
	});
	
	eg::console::AddCommand("unlock", 1, [&] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		for (Level& level : levels)
		{
			if ((level.name == args[0] || args[0] == "all") && level.status == LevelStatus::Locked)
				level.status = LevelStatus::Unlocked;
		}
	});
	
	eg::console::SetCompletionProvider("unlock", 0, [] (eg::Span<const std::string_view> args, eg::console::CompletionsList& list)
	{
		for (const Level& level : levels)
			list.Add(level.name);
		list.Add("all");
	});
	
	eg::console::AddCommand("play", 1, [this] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		int64_t levelIndex = FindLevel(args[0]);
		if (levelIndex == -1)
		{
			writer.Write(eg::console::ErrorColor.ScaleRGB(0.8f), "Level not found: ");
			writer.WriteLine(eg::console::ErrorColor, args[0]);
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
	
	eg::console::AddCommand("updateThumbnails", 0, [&] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		m_levelThumbnailUpdateFrameIdx = eg::FrameIdx();
		m_levelThumbnailUpdate = BeginUpdateLevelThumbnails(m_renderCtx, writer);
	});
	
	InitializeWallShader();
}

Game::~Game()
{
	delete editor;
	delete mainGameState;
	delete mainMenuGameState;
	delete RenderSettings::instance;
}

void Game::ResolutionChanged(int newWidth, int newHeight)
{
	editor->SetResolution(newWidth, newHeight);
}

void Game::RunFrame(float dt)
{
	if (m_levelThumbnailUpdate != nullptr && eg::FrameIdx() > m_levelThumbnailUpdateFrameIdx + 4)
	{
		EndUpdateLevelThumbnails(m_levelThumbnailUpdate);
		m_levelThumbnailUpdate = nullptr;
	}
	
	m_imGuiInterface.NewFrame();
	
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
