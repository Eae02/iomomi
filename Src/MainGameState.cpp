#include "MainGameState.hpp"
#include "Settings.hpp"
#include "Levels.hpp"
#include "World/Entities/EntTypes/EntranceExitEnt.hpp"
#include "MainMenuGameState.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

static int* relativeMouseMode = eg::TweakVarInt("relms", 1, 0, 1);

MainGameState::MainGameState()
{
	eg::console::AddCommand("reload", 0, [this] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		if (!ReloadLevel())
		{
			writer.WriteLine(eg::console::ErrorColor, "No level to reload");
		}
		else
		{
			eg::console::Hide();
		}
	});
	
	eg::console::AddCommand("curlevel", 0, [this] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
	{
		if (m_currentLevelIndex == -1)
		{
			writer.WriteLine(eg::console::ErrorColor, "No current level");
		}
		else
		{
			writer.WriteLine(eg::console::InfoColor, levels[m_currentLevelIndex].name);
		}
	});
	
	eg::console::AddCommand("showExtraLevels", 0, [this] (eg::Span<const std::string_view>, eg::console::Writer&)
	{
		settings.showExtraLevels = true;
	});
	
	const eg::Texture& particlesTexture = eg::GetAsset<eg::Texture>("Textures/Particles.png");
	m_particleManager.SetTextureSize(particlesTexture.Width(), particlesTexture.Height());
	
	m_playerWaterAABB = std::make_shared<WaterSimulator::QueryAABB>();
	GameRenderer::instance->m_waterSimulator.AddQueryAABB(m_playerWaterAABB);
}

void MainGameState::SetWorld(std::unique_ptr<World> newWorld, int64_t levelIndex, const EntranceExitEnt* exitEntity)
{
	m_player.Reset();
	m_gameTime = 0;
	m_currentLevelIndex = levelIndex;
	m_pausedMenu.isPaused = false;
	m_pausedMenu.shouldRestartLevel = false;
	
	//Moves the player to the entrance entity for this level
	newWorld->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
	{
		if (entity.m_type == EntranceExitEnt::Type::Entrance)
		{
			if (exitEntity != nullptr)
			{
				EntranceExitEnt::MovePlayer(*exitEntity, entity, m_player);
			}
			else
			{
				entity.InitPlayer(m_player);
			}
		}
	});
	
	m_world = std::move(newWorld);
	
	GameRenderer::instance->WorldChanged(*m_world);
	
	if (levelIndex != -1)
	{
		mainMenuGameState->backgroundLevel = &levels[levelIndex];
	}
}

void MainGameState::OnDeactivate()
{
	GameRenderer::instance->m_waterSimulator.Stop();
	m_world.reset();
}

void MainGameState::RunFrame(float dt)
{
	GameRenderer::instance->m_player = &m_player;
	GameRenderer::instance->m_physicsDebugRenderer = &m_physicsDebugRenderer;
	GameRenderer::instance->m_physicsEngine = &m_physicsEngine;
	GameRenderer::instance->m_particleManager = &m_particleManager;
	GameRenderer::instance->m_gravityGun = &m_gravityGun;
	GameRenderer::instance->postColorScale = glm::mix(1.0f, 0.5f, m_pausedMenu.fade);
	
	constexpr float MAX_DT = 0.2f;
	if (dt > MAX_DT)
		dt = MAX_DT;
	
	auto UpdateViewProjMatrices = [&] ()
	{
		glm::mat4 viewMatrix, inverseViewMatrix;
		m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
		GameRenderer::instance->SetViewMatrix(viewMatrix, inverseViewMatrix);
	};
	
	m_pausedMenu.Update(dt);
	if (m_world == nullptr)
		return;
	
	if (!eg::console::IsShown() && !settingsWindowVisible && !m_pausedMenu.isPaused)
	{
		auto worldUpdateCPUTimer = eg::StartCPUTimer("World Update");
		
		EntranceExitEnt* currentExit = nullptr;
		
		m_world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
		{
			if (entity.ShouldSwitchEntrance())
				currentExit = &entity;
		});
		
		//Moves to the next level
		if (currentExit != nullptr && m_currentLevelIndex != -1 && levels[m_currentLevelIndex].nextLevelIndex != -1)
		{
			MarkLevelCompleted(levels[m_currentLevelIndex]);
			int64_t nextLevelIndex = levels[m_currentLevelIndex].nextLevelIndex;
			eg::Log(eg::LogLevel::Info, "lvl", "Going to next level '{0}'", levels[nextLevelIndex].name);
			SetWorld(LoadLevelWorld(levels[nextLevelIndex], false), nextLevelIndex, currentExit);
			m_physicsEngine = {};
		}
		
		WorldUpdateArgs updateArgs;
		updateArgs.mode = WorldMode::Game;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.waterSim = &GameRenderer::instance->m_waterSimulator;
		updateArgs.physicsEngine = &m_physicsEngine;
		updateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { GameRenderer::instance->InvalidateShadows(sphere); };
		
		eg::SetRelativeMouseMode(*relativeMouseMode);
		
		m_physicsEngine.BeginCollect();
		m_world->CollectPhysicsObjects(m_physicsEngine, dt);
		m_physicsEngine.EndCollect();
		
		{
			auto playerUpdateCPUTimer = eg::StartCPUTimer("Player Update");
			bool underwater = m_playerWaterAABB->GetResults().numIntersecting > 30;
			m_player.Update(*m_world, m_physicsEngine, dt, underwater);
		}
		
		m_world->Update(updateArgs, &m_physicsEngine);
		
		UpdateViewProjMatrices();
		if (m_world->playerHasGravityGun)
		{
			m_gravityGun.Update(*m_world, m_physicsEngine, GameRenderer::instance->m_waterSimulator, m_particleManager,
			                    m_player, GameRenderer::instance->GetInverseViewProjMatrix(), dt);
		}
	}
	else
	{
		eg::SetRelativeMouseMode(false);
		UpdateViewProjMatrices();
	}
	
	m_playerWaterAABB->SetAABB(m_player.GetAABB());
	
	GameRenderer::instance->Render(*m_world, m_gameTime, dt, nullptr, eg::CurrentResolutionX(), eg::CurrentResolutionY());
	
	const float CROSSHAIR_SIZE = 32;
	const float CROSSHAIR_OPACITY = 0.75f;
	eg::SpriteBatch::overlay.Draw(eg::GetAsset<eg::Texture>("Textures/Crosshair.png"),
	    eg::Rectangle::CreateCentered(eg::CurrentResolutionX() / 2.0f, eg::CurrentResolutionY() / 2.0f, CROSSHAIR_SIZE, CROSSHAIR_SIZE),
	    eg::ColorLin(eg::Color::White.ScaleAlpha(CROSSHAIR_OPACITY)), eg::SpriteFlags::None);
	
	m_pausedMenu.Draw(eg::SpriteBatch::overlay);
	if (m_pausedMenu.shouldRestartLevel)
	{
		ReloadLevel();
	}
	
#ifndef NDEBUG
	if (eg::DevMode())
	{
		DrawOverlay(dt);
	}
#endif
	
	if (!m_pausedMenu.isPaused)
	{
		m_gameTime += dt;
	}
}

int* debugOverlay = eg::TweakVarInt("dbg_overlay", 1);

#ifndef NDEBUG
void MainGameState::DrawOverlay(float dt)
{
	if (!*debugOverlay)
		return;
	
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
	ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);
	
	const char* graphicsAPIName = "?";
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL)
		graphicsAPIName = "OpenGL";
	else if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan)
		graphicsAPIName = "Vulkan";
	
	ImGui::Text("FPS: %dHz | %.2fms", (int)(1.0f / dt), dt * 1000.0f);
	ImGui::Text("Graphics API: %s", graphicsAPIName);
	ImGui::Separator();
	m_player.DrawDebugOverlay();
	ImGui::Separator();
	ImGui::Text("Particles: %d", m_particleManager.ParticlesToDraw());
	//ImGui::Text("PSM Last Update: %ld/%ld", m_plShadowMapper.LastUpdateFrameIndex(), eg::FrameIdx());
	//ImGui::Text("PSM Updates: %ld", m_plShadowMapper.LastFrameUpdateCount());
	ImGui::Text("Water Spheres: %d", GameRenderer::instance->m_waterSimulator.NumParticles());
	ImGui::Text("Water Update Time: %.2fms", GameRenderer::instance->m_waterSimulator.LastUpdateTime() / 1E6);
	
	ImGui::End();
	ImGui::PopStyleVar();
}
#endif

bool MainGameState::ReloadLevel()
{
	if (m_currentLevelIndex == -1)
		return false;
	SetWorld(LoadLevelWorld(levels[m_currentLevelIndex], false), m_currentLevelIndex);
	return true;
}
