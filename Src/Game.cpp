#include "Game.hpp"

#include <fstream>

#include "Editor/Editor.hpp"
#include "GameState.hpp"
#include "Graphics/GraphicsCommon.hpp"
#include "Graphics/WallShader.hpp"
#include "ImGui.hpp"
#include "Levels.hpp"
#include "MainGameState.hpp"
#include "MainMenuGameState.hpp"
#include "ThumbnailRenderer.hpp"

pcg32_fast globalRNG;

Game::Game()
{
	globalRNG = pcg32_fast(std::time(nullptr));
	GameRenderer::instance = new GameRenderer(m_renderCtx);

#ifdef IOMOMI_ENABLE_EDITOR
	editor = new Editor(m_renderCtx);
	m_editorGameState = editor;
#endif

#ifdef EG_HAS_IMGUI
	eg::imgui::InitializeArgs imguiInitArgs;
	imguiInitArgs.enableImGuiIni = eg::DevMode();
	eg::imgui::Initialize(imguiInitArgs);
#endif

	mainGameState = new MainGameState;
	mainMenuGameState = new MainMenuGameState();

	SetCurrentGS(mainMenuGameState);

	eg::console::AddCommand(
		"ed", 0,
		[&](std::span<const std::string_view> args, eg::console::Writer& writer)
		{
			if (m_editorGameState != nullptr)
			{
				SetCurrentGS(m_editorGameState);
				eg::console::Hide();
			}
			else
			{
				writer.WriteLine(
					eg::console::ErrorColor, "This version of iomomi was not compiled with the editor enabled");
			}
		});

#ifndef __EMSCRIPTEN__
	eg::console::AddCommand(
		"updateThumbnails", 0,
		[&](std::span<const std::string_view> args, eg::console::Writer& writer)
		{
			m_levelThumbnailUpdateFrameIdx = eg::FrameIdx();
			m_levelThumbnailUpdate = BeginUpdateLevelThumbnails(m_renderCtx, writer);
		});
#endif

	eg::console::AddCommand(
		"mm", 0,
		[&](std::span<const std::string_view> args, eg::console::Writer& writer)
		{
			mainMenuGameState->GoToMainScreen();
			SetCurrentGS(mainMenuGameState);
			eg::console::Hide();
		});

	eg::console::AddCommand(
		"compl", 1,
		[&](std::span<const std::string_view> args, eg::console::Writer& writer)
		{
			if (args[0] == "reset")
			{
				for (Level& level : levels)
				{
					level.status = level.isExtra ? LevelStatus::Unlocked : LevelStatus::Locked;
				}
				levels[FindLevel(levelsOrder[0])].status = LevelStatus::Unlocked;
				SaveProgress();
			}
			else if (args[0] == "all")
			{
				for (Level& level : levels)
				{
					level.status = LevelStatus::Completed;
				}
				SaveProgress();
			}
			else
			{
				int64_t levelIndex = FindLevel(args[0]);
				if (levelIndex != -1)
				{
					MarkLevelCompleted(levels[levelIndex]);
				}
				else
				{
					writer.WriteLine(eg::console::ErrorColor, "Level not found");
				}
			}
		});

	eg::console::SetCompletionProvider(
		"compl", 0,
		[](std::span<const std::string_view> args, eg::console::CompletionsList& list)
		{
			for (const Level& level : levels)
				list.Add(level.name);
			list.Add("all");
			list.Add("reset");
		});

	eg::console::AddCommand(
		"play", 1,
		[](std::span<const std::string_view> args, eg::console::Writer& writer)
		{
			int64_t levelIndex = FindLevel(args[0]);
			if (levelIndex == -1)
			{
				writer.Write(eg::console::ErrorColor.ScaleRGB(0.8f), "Level not found: ");
				writer.WriteLine(eg::console::ErrorColor, args[0]);
				return;
			}

			if (std::unique_ptr<World> world = LoadLevelWorld(levels[levelIndex], false))
			{
				SetCurrentGS(mainGameState);
				mainGameState->SetWorld(std::move(world), levelIndex);
			}

			eg::console::Hide();
		});

	eg::console::SetCompletionProvider(
		"play", 0,
		[](std::span<const std::string_view> args, eg::console::CompletionsList& list)
		{
			for (const Level& level : levels)
				list.Add(level.name);
		});

	InitializeWallShader();
}

Game::~Game()
{
	delete m_editorGameState;
	delete mainGameState;
	delete mainMenuGameState;
	delete RenderSettings::instance;
	delete GameRenderer::instance;
}

static float* minSimulationFramerate = eg::TweakVarFloat("min_sim_fps", 20, 0);

void Game::RunFrame(float dt)
{
	if (*minSimulationFramerate > 1E-5f)
	{
		dt = std::min(dt, 1.0f / *minSimulationFramerate);
	}

	if (m_levelThumbnailUpdate != nullptr && eg::FrameIdx() > m_levelThumbnailUpdateFrameIdx + 4)
	{
		EndUpdateLevelThumbnails(m_levelThumbnailUpdate);
		m_levelThumbnailUpdate = nullptr;
	}

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

	m_gameTime += dt;
}
