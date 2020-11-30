#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/RenderSettings.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "Graphics/Materials/GravitySwitchVolLightMaterial.hpp"
#include "Settings.hpp"
#include "Levels.hpp"
#include "World/Entities/EntTypes/EntranceExitEnt.hpp"
#include "World/Entities/EntTypes/ActivationLightStripEnt.hpp"
#include "World/Entities/EntTypes/CubeEnt.hpp"
#include "World/Entities/EntTypes/WindowEnt.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

static int* relativeMouseMode = eg::TweakVarInt("relms", 1, 0, 1);

MainGameState::MainGameState(RenderContext& renderCtx) : m_renderer(renderCtx)
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
	
	const eg::Texture& particlesTexture = eg::GetAsset<eg::Texture>("Textures/Particles.png");
	m_particleManager.SetTextureSize(particlesTexture.Width(), particlesTexture.Height());
	
	m_playerWaterAABB = std::make_shared<WaterSimulator::QueryAABB>();
	m_renderer.m_waterSimulator.AddQueryAABB(m_playerWaterAABB);
	
	m_renderer.m_player = &m_player;
	m_renderer.m_physicsDebugRenderer = &m_physicsDebugRenderer;
	m_renderer.m_physicsEngine = &m_physicsEngine;
	m_renderer.m_particleManager = &m_particleManager;
	m_renderer.m_gravityGun = &m_gravityGun;
}

void MainGameState::LoadWorld(std::istream& stream, int64_t levelIndex, const EntranceExitEnt* exitEntity)
{
	auto newWorld = World::Load(stream, false);
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
	
	ActivationLightStripEnt::GenerateAll(*m_world);
	
	m_renderer.WorldChanged(*m_world);
}

void MainGameState::OnDeactivate()
{
	m_renderer.m_waterSimulator.Stop();
	m_world.reset();
}

void MainGameState::RunFrame(float dt)
{
	constexpr float MAX_DT = 0.2f;
	if (dt > MAX_DT)
		dt = MAX_DT;
	
	auto UpdateViewProjMatrices = [&] ()
	{
		glm::mat4 viewMatrix, inverseViewMatrix;
		m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
		m_renderer.SetViewMatrix(viewMatrix, inverseViewMatrix);
	};
	
	m_pausedMenu.Update(dt);
	if (m_world == nullptr)
		return;
	
	if (!eg::console::IsShown() && !settingsWindowVisible && !m_pausedMenu.isPaused)
	{
		auto worldUpdateCPUTimer = eg::StartCPUTimer("World Update");
		
		WorldUpdateArgs updateArgs;
		updateArgs.mode = WorldMode::Game;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.waterSim = &m_renderer.m_waterSimulator;
		updateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { m_renderer.InvalidateShadows(sphere); };
		
		eg::SetRelativeMouseMode(*relativeMouseMode);
		
		m_physicsEngine.BeginCollect();
		
		m_world->CollectPhysicsObjects(m_physicsEngine, dt);
		
		m_physicsEngine.EndCollect();
		
		{
			auto playerUpdateCPUTimer = eg::StartCPUTimer("Player Update");
			bool underwater = m_playerWaterAABB->GetResults().numIntersecting > 30;
			m_player.Update(*m_world, m_physicsEngine, dt, underwater);
		}
		
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.physicsEngine = &m_physicsEngine;
		m_world->Update(updateArgs);
		
		{
			auto playerUpdateCPUTimer = eg::StartCPUTimer("Physics");
			m_physicsEngine.Simulate(dt);
		}
		
		m_world->entManager.ForEachOfType<CubeEnt>([&] (CubeEnt& cube)
		{
			cube.UpdatePostSim(updateArgs);
		});
		
		UpdateViewProjMatrices();
		if (m_world->playerHasGravityGun)
		{
			m_gravityGun.Update(*m_world, m_physicsEngine, m_renderer.m_waterSimulator, m_particleManager,
			                    m_player, m_renderer.GetInverseViewProjMatrix(), dt);
		}
		
		EntranceExitEnt* currentExit = nullptr;
		
		m_world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
		{
			entity.Update(updateArgs);
			if (entity.ShouldSwitchEntrance())
				currentExit = &entity;
		});
		
		//Moves to the next level
		if (currentExit != nullptr && m_currentLevelIndex != -1 && levels[m_currentLevelIndex].nextLevelIndex != -1)
		{
			MarkLevelCompleted(levels[m_currentLevelIndex]);
			int64_t nextLevelIndex = levels[m_currentLevelIndex].nextLevelIndex;
			eg::Log(eg::LogLevel::Info, "lvl", "Going to next level '{0}'", levels[nextLevelIndex].name);
			std::string path = GetLevelPath(levels[nextLevelIndex].name);
			std::ifstream stream(path, std::ios::binary);
			LoadWorld(stream, nextLevelIndex, currentExit);
		}
	}
	else
	{
		eg::SetRelativeMouseMode(false);
		UpdateViewProjMatrices();
	}
	
	m_playerWaterAABB->SetAABB(m_player.GetAABB());
	
	m_renderer.Render(*m_world, m_gameTime, dt, nullptr, eg::CurrentResolutionX(), eg::CurrentResolutionY());
	
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
	ImGui::Text("Water Spheres: %d", m_renderer.m_waterSimulator.NumParticles());
	ImGui::Text("Water Update Time: %.2fms", m_renderer.m_waterSimulator.LastUpdateTime() / 1E6);
	
	ImGui::End();
	ImGui::PopStyleVar();
}
#endif

bool MainGameState::ReloadLevel()
{
	if (m_currentLevelIndex == -1)
		return false;
	
	std::string levelPath = GetLevelPath(levels[m_currentLevelIndex].name);
	std::ifstream levelStream(levelPath, std::ios::binary);
	LoadWorld(levelStream, m_currentLevelIndex);
	
	return true;
}
