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

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

static int* relativeMouseMode = eg::TweakVarInt("relms", 1, 0, 1);
static int* physicsDebug = eg::TweakVarInt("phys_dbg_draw", 0, 0, 1);

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_projection.SetZNear(0.02f);
	m_projection.SetZFar(200.0f);
	
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
	
	m_particleManager.SetTextureSize(1024, 1024);
	
	m_playerWaterAABB = std::make_shared<WaterSimulator::QueryAABB>();
	m_waterSimulator.AddQueryAABB(m_playerWaterAABB);
}

void MainGameState::LoadWorld(std::istream& stream, int64_t levelIndex, const EntranceExitEnt* exitEntity)
{
	auto newWorld = World::Load(stream, false);
	m_player.Reset();
	m_gameTime = 0;
	m_currentLevelIndex = levelIndex;
	
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
	
	m_waterSimulator.Init(*m_world);
	
	m_plShadowMapper.InvalidateAll();
}

void MainGameState::OnDeactivate()
{
	m_waterSimulator.Stop();
}

void MainGameState::RunFrame(float dt)
{
	constexpr float MAX_DT = 0.2f;
	if (dt > MAX_DT)
		dt = MAX_DT;
	
	glm::mat4 viewMatrix, inverseViewMatrix, viewProjMatrix, inverseViewProjMatrix;
	auto UpdateViewProjMatrices = [&] ()
	{
		m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
		viewProjMatrix = m_projection.Matrix() * viewMatrix;
		inverseViewProjMatrix = inverseViewMatrix * m_projection.InverseMatrix();
	};
	
	m_pausedMenu.Update(dt);
	if (m_world == nullptr)
		return;
	
	if (!eg::console::IsShown() && !settingsWindowVisible && !m_pausedMenu.isPaused)
	{
		auto worldUpdateCPUTimer = eg::StartCPUTimer("World Update");
		
		WorldUpdateArgs updateArgs;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.waterSim = &m_waterSimulator;
		updateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { m_plShadowMapper.Invalidate(sphere); };
		
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
			m_gravityGun.Update(*m_world, m_physicsEngine, m_waterSimulator, m_particleManager, m_player, inverseViewProjMatrix, dt);
		}
		
		EntranceExitEnt* currentExit = nullptr;
		
		m_world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
		{
			entity.Update(updateArgs);
			if (entity.ShouldSwitchEntrance())
				currentExit = &entity;
		});
		
		if (currentExit != nullptr && m_currentLevelIndex != -1 && levels[m_currentLevelIndex].nextLevelIndex != -1)
		{
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
	
	{
		auto waterUpdateTimer = eg::StartCPUTimer("Water Update MT");
		
		m_playerWaterAABB->SetAABB(m_player.GetAABB());
		
		m_waterSimulator.Update(*m_world);
	}
	
	eg::Frustum frustum(inverseViewProjMatrix);
	
	if (m_lastSettingsGeneration != SettingsGeneration())
	{
		m_projection.SetFieldOfViewDeg(settings.fieldOfViewDeg);
		
		m_plShadowMapper.SetQuality(settings.shadowQuality);
		
		GravitySwitchVolLightMaterial::SetQuality(settings.lightingQuality);
		
		m_lastSettingsGeneration = SettingsGeneration();
		
		if (m_lightingQuality != settings.lightingQuality)
		{
			m_lightingQuality = settings.lightingQuality;
			m_bloomRenderTarget.reset();
		}
	}
	
	GravityCornerLightMaterial::instance.Update(dt);
	
	if (settings.BloomEnabled())
	{
		bool outOfDate = m_bloomRenderTarget == nullptr ||
			(int)m_bloomRenderTarget->InputWidth() != eg::CurrentResolutionX() ||
			(int)m_bloomRenderTarget->InputHeight() != eg::CurrentResolutionY();
		
		if (outOfDate)
		{
			m_bloomRenderTarget = std::make_unique<eg::BloomRenderer::RenderTarget>(
				(uint32_t)eg::CurrentResolutionX(), (uint32_t)eg::CurrentResolutionY(), 3);
			
			if (m_bloomRenderer == nullptr)
			{
				m_bloomRenderer = std::make_unique<eg::BloomRenderer>();
			}
		}
	}
	
	auto cpuTimerPrepare = eg::StartCPUTimer("Prepare Draw");
	
	RenderSettings::instance->gameTime = m_gameTime;
	RenderSettings::instance->cameraPosition = m_player.EyePosition();
	RenderSettings::instance->viewProjection = viewProjMatrix;
	RenderSettings::instance->invViewProjection = inverseViewProjMatrix;
	RenderSettings::instance->viewMatrix = viewMatrix;
	RenderSettings::instance->invViewMatrix = inverseViewMatrix;
	RenderSettings::instance->projectionMatrix = m_projection.Matrix();
	RenderSettings::instance->invProjectionMatrix = m_projection.InverseMatrix();
	RenderSettings::instance->UpdateBuffer();
	
	m_renderCtx->meshBatch.Begin();
	m_renderCtx->transparentMeshBatch.Begin();
	
	PrepareDrawArgs prepareDrawArgs;
	prepareDrawArgs.isEditor = false;
	prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	prepareDrawArgs.transparentMeshBatch = &m_renderCtx->transparentMeshBatch;
	prepareDrawArgs.player = &m_player;
	prepareDrawArgs.spotLights.clear();
	prepareDrawArgs.pointLights.clear();
	m_world->PrepareForDraw(prepareDrawArgs);
	if (m_world->playerHasGravityGun)
	{
		m_gravityGun.CollectLights(prepareDrawArgs.pointLights);
		m_gravityGun.Draw(m_renderCtx->meshBatch);
	}
	
	m_renderCtx->transparentMeshBatch.End(eg::DC);
	m_renderCtx->meshBatch.End(eg::DC);
	
	cpuTimerPrepare.Stop();
	
	m_particleManager.Step(dt, frustum, m_player.Rotation() * glm::vec3(0, 0, -1));
	
	m_plShadowMapper.UpdateShadowMaps(prepareDrawArgs.pointLights, [this] (const PointLightShadowRenderArgs& args)
	{
		RenderPointLightShadows(args);
	});
	
	MeshDrawArgs mDrawArgs;
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		mDrawArgs.waterDepthTexture = GetRenderTexture(RenderTex::WaterDepthBlurred2);
	}
	else
	{
		mDrawArgs.waterDepthTexture = WaterRenderer::GetDummyDepthTexture();
		RedirectRenderTexture(RenderTex::LitWithoutWater, RenderTex::LitWithoutSSR);
	}
	
	if (!settings.SSREnabled())
	{
		RedirectRenderTexture(RenderTex::LitWithoutSSR, RenderTex::Lit);
	}
	
	{
		auto gpuTimerGeom = eg::StartGPUTimer("Geometry");
		auto cpuTimerGeom = eg::StartCPUTimer("Geometry");
		eg::DC.DebugLabelBegin("Geometry");
		
		m_renderCtx->renderer.BeginGeometry();
		
		m_world->Draw();
		mDrawArgs.drawMode = MeshDrawMode::Game;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		eg::DC.DebugLabelEnd();
		eg::DC.DebugLabelBegin("Geometry Flags");
		
		m_renderCtx->renderer.BeginGeometryFlags();
		
		mDrawArgs.drawMode = MeshDrawMode::ObjectFlags;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_renderCtx->renderer.EndGeometry();
		eg::DC.DebugLabelEnd();
	}
	
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		auto gpuTimerWater = eg::StartGPUTimer("Water (early)");
		auto cpuTimerWater = eg::StartCPUTimer("Water (early)");
		eg::DC.DebugLabelBegin("Water (early)");
		m_waterRenderer.RenderEarly(m_waterSimulator.GetPositionsBuffer(), m_waterSimulator.NumParticlesToDraw());
		eg::DC.DebugLabelEnd();
	}
	
	{
		auto gpuTimerLight = eg::StartGPUTimer("Lighting");
		auto cpuTimerLight = eg::StartCPUTimer("Lighting");
		eg::DC.DebugLabelBegin("Lighting");
		
		m_renderCtx->renderer.BeginLighting();
		
		m_renderCtx->renderer.DrawSpotLights(prepareDrawArgs.spotLights);
		m_renderCtx->renderer.DrawPointLights(prepareDrawArgs.pointLights);
		
		m_particleRenderer.Draw(m_particleManager, GetRenderTexture(RenderTex::GBDepth));
		
		m_renderCtx->renderer.End();
		eg::DC.DebugLabelEnd();
	}
	
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (pre-water)");
		auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (pre-water)");
		
		m_renderCtx->renderer.BeginTransparent(RenderTex::LitWithoutWater);
		
		eg::DC.DebugLabelBegin("Emissive");
		
		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		eg::DC.DebugLabelEnd();
		eg::DC.DebugLabelBegin("Transparent (pre-water)");
		
		mDrawArgs.drawMode = MeshDrawMode::TransparentBeforeWater;
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
		
		eg::DC.DebugLabelEnd();
		
		m_renderCtx->renderer.EndTransparent();
		RenderTextureUsageHintFS(RenderTex::GBDepth);
	}
	
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		RenderTextureUsageHintFS(RenderTex::LitWithoutWater);
		
		auto gpuTimerWater = eg::StartGPUTimer("Water (post)");
		auto cpuTimerWater = eg::StartCPUTimer("Water (post)");
		eg::DC.DebugLabelBegin("Water (post)");
		m_waterRenderer.RenderPost();
		eg::DC.DebugLabelEnd();
	}
	else
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Emissive");
		auto cpuTimerTransparent = eg::StartCPUTimer("Emissive");
		eg::DC.DebugLabelBegin("Emissive");
		
		m_renderCtx->renderer.BeginTransparent(RenderTex::LitWithoutSSR);
		
		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_renderCtx->renderer.EndTransparent();
		RenderTextureUsageHintFS(RenderTex::GBDepth);
		
		eg::DC.DebugLabelEnd();
	}
	
	if (settings.SSREnabled())
	{
		RenderTextureUsageHintFS(RenderTex::LitWithoutSSR);
		
		auto gpuTimerTransparent = eg::StartGPUTimer("SSR");
		eg::DC.DebugLabelBegin("SSR");
		
		m_ssr.Render(mDrawArgs.waterDepthTexture);
		
		eg::DC.DebugLabelEnd();
	}
	
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Transparent");
		auto cpuTimerTransparent = eg::StartCPUTimer("Transparent");
		eg::DC.DebugLabelBegin("Transparent");
		
		m_renderCtx->renderer.BeginTransparent(RenderTex::Lit);
		
		mDrawArgs.drawMode = MeshDrawMode::TransparentAfterWater;
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_renderCtx->renderer.EndTransparent();
		eg::DC.DebugLabelEnd();
	}
	
	//TODO: Move gun rendering here
	
	if (*physicsDebug)
	{
		m_physicsDebugRenderer.Render(m_physicsEngine, viewProjMatrix,
			GetRenderTexture(RenderTex::Lit), GetRenderTexture(RenderTex::GBDepth));
	}
	
	RenderTextureUsageHintFS(RenderTex::Lit);
	
	eg::DC.DebugLabelBegin("Post");
	
	if (m_bloomRenderTarget != nullptr)
	{
		auto gpuTimerBloom = eg::StartGPUTimer("Bloom");
		auto cpuTimerBloom = eg::StartCPUTimer("Bloom");
		m_bloomRenderer->Render(glm::vec3(1.5f), GetRenderTexture(RenderTex::Lit), *m_bloomRenderTarget);
	}
	
	{
		auto gpuTimerPost = eg::StartGPUTimer("Post");
		auto cpuTimerPost = eg::StartCPUTimer("Post");
		m_postProcessor.Render(GetRenderTexture(RenderTex::Lit), m_bloomRenderTarget.get());
	}
	
	eg::DC.DebugLabelEnd();
	
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

void MainGameState::RenderPointLightShadows(const PointLightShadowRenderArgs& args)
{
	m_world->DrawPointLightShadows(args);
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.drawMode = MeshDrawMode::PointLightShadow;
	mDrawArgs.plShadowRenderArgs = &args;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
}

#ifndef NDEBUG
void MainGameState::DrawOverlay(float dt)
{
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
	ImGui::Text("Particles: %d", m_particleManager.ParticlesToDraw());
	ImGui::Text("Water Spheres: %d", m_waterSimulator.NumParticles());
	ImGui::Text("Water Update Time: %.2fms", m_waterSimulator.LastUpdateTime() / 1E6);
	
	ImGui::End();
	ImGui::PopStyleVar();
}
#endif

void MainGameState::SetResolution(int width, int height)
{
	m_projection.SetResolution(width, height);
}

bool MainGameState::ReloadLevel()
{
	if (m_currentLevelIndex == -1)
		return false;
	
	std::string levelPath = GetLevelPath(levels[m_currentLevelIndex].name);
	std::ifstream levelStream(levelPath, std::ios::binary);
	LoadWorld(levelStream, m_currentLevelIndex);
	
	return true;
}
