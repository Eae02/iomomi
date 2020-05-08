#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/RenderSettings.hpp"
#include "Graphics/PlanarReflectionsManager.hpp"
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
	m_prepareDrawArgs.isEditor = false;
	m_prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	m_prepareDrawArgs.transparentMeshBatch = &m_renderCtx->transparentMeshBatch;
	m_prepareDrawArgs.player = &m_player;
	m_projection.SetZNear(0.02f);
	m_projection.SetZFar(200.0f);
	
	eg::console::AddCommand("reload", 0, [this] (eg::Span<const std::string_view> args)
	{
		if (!ReloadLevel())
		{
			eg::Log(eg::LogLevel::Error, "lvl", "No level to reload");
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

void MainGameState::DoDeferredRendering(bool useLightProbes, DeferredRenderer::RenderTarget& renderTarget)
{
	MeshDrawArgs mDrawArgs;
	mDrawArgs.renderTarget = m_renderTarget.get();
	
	{
		auto gpuTimerGeom = eg::StartGPUTimer("Geometry");
		auto cpuTimerGeom = eg::StartCPUTimer("Geometry");
		
		m_renderCtx->renderer.BeginGeometry(renderTarget);
		
		m_world->Draw();
		
		mDrawArgs.drawMode = MeshDrawMode::Game;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	}
	
	{
		auto gpuTimerEmi = eg::StartGPUTimer("Emissive");
		auto cpuTimerEmi = eg::StartCPUTimer("Emissive");
		
		m_renderCtx->renderer.BeginEmissive(renderTarget, m_waterSimulator.NumParticles() > 0);
		
		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
	}
	
	{
		auto gpuTimerLight = eg::StartGPUTimer("Lighting");
		auto cpuTimerLight = eg::StartCPUTimer("Lighting");
		
		m_renderCtx->renderer.BeginLighting(renderTarget, m_waterSimulator.NumParticles() > 0);
		
		m_renderCtx->renderer.DrawReflectionPlaneLighting(renderTarget, m_prepareDrawArgs.reflectionPlanes);
		m_renderCtx->renderer.DrawSpotLights(renderTarget, m_prepareDrawArgs.spotLights);
		m_renderCtx->renderer.DrawPointLights(renderTarget, m_prepareDrawArgs.pointLights);
		
		mDrawArgs.drawMode = MeshDrawMode::Transparent;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_particleRenderer.Draw(m_particleManager, renderTarget.DepthTexture());
		
		m_renderCtx->renderer.End(renderTarget);
	}
	
	if (m_waterSimulator.NumParticles() > 0)
	{
		auto gpuTimerWater = eg::StartGPUTimer("Draw Water");
		auto cpuTimerWater = eg::StartCPUTimer("Draw Water");
		
		m_renderCtx->renderer.DrawWater(renderTarget, m_waterSimulator.GetPositionsBuffer(), m_waterSimulator.NumParticles());
	}
}

void MainGameState::RenderPlanarReflections(const ReflectionPlane& plane, eg::FramebufferRef framebuffer)
{
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = framebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::Color::Black);
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world->DrawPlanarReflections(plane.plane);
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.drawMode = MeshDrawMode::PlanarReflection;
	mDrawArgs.reflectionPlane = plane.plane;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	eg::DC.EndRenderPass();
}

void MainGameState::RunFrame(float dt)
{
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
		
		if (currentExit != nullptr && m_currentLevelIndex != -1)
		{
			const std::string& exitName = currentExit->GetName();
			int64_t nextLevelIndex = GetNextLevelIndex(m_currentLevelIndex, exitName);
			if (nextLevelIndex != -1)
			{
				eg::Log(eg::LogLevel::Info, "lvl", "Going to next level '{0}'", levels[nextLevelIndex].name);
				std::string path = GetLevelPath(levels[nextLevelIndex].name);
				std::ifstream stream(path, std::ios::binary);
				LoadWorld(stream, nextLevelIndex, currentExit);
			}
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
		
		m_waterSimulator.Update();
	}
	
	eg::Frustum frustum(inverseViewProjMatrix);
	
	if (m_lastSettingsGeneration != SettingsGeneration())
	{
		m_projection.SetFieldOfViewDeg(settings.fieldOfViewDeg);
		
		m_planarReflectionsManager.SetQuality(settings.reflectionsQuality);
		m_plShadowMapper.SetQuality(settings.shadowQuality);
		
		GravitySwitchVolLightMaterial::SetQuality(settings.lightingQuality);
		
		m_lastSettingsGeneration = SettingsGeneration();
		
		if (m_lightingQuality != settings.lightingQuality)
		{
			m_lightingQuality = settings.lightingQuality;
			m_renderTarget.reset();
			m_bloomRenderTarget.reset();
		}
		
		if (m_waterQuality != settings.waterQuality)
		{
			m_renderTarget.reset();
			m_waterQuality = settings.waterQuality;
		}
	}
	
	GravityCornerLightMaterial::instance.Update(dt);
	
	if (m_renderTarget == nullptr || m_renderTarget->Width() != (uint32_t)eg::CurrentResolutionX() ||
		m_renderTarget->Height() != (uint32_t)eg::CurrentResolutionY())
	{
		eg::TextureCreateInfo textureCI;
		if (settings.HDREnabled())
			textureCI.format = DeferredRenderer::LIGHT_COLOR_FORMAT_HDR;
		else
			textureCI.format = DeferredRenderer::LIGHT_COLOR_FORMAT_LDR;
		textureCI.width = (uint32_t)eg::CurrentResolutionX();
		textureCI.height = (uint32_t)eg::CurrentResolutionY();
		textureCI.mipLevels = 1;
		textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment;
		m_renderOutputTexture = eg::Texture::Create2D(textureCI);
		
		m_renderTarget = std::make_unique<DeferredRenderer::RenderTarget>((uint32_t)eg::CurrentResolutionX(),
			(uint32_t)eg::CurrentResolutionY(), m_renderOutputTexture, 0, m_waterQuality);
		
		if (settings.BloomEnabled())
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
	
	m_prepareDrawArgs.spotLights.clear();
	m_prepareDrawArgs.pointLights.clear();
	m_prepareDrawArgs.reflectionPlanes.clear();
	m_world->PrepareForDraw(m_prepareDrawArgs);
	if (m_world->playerHasGravityGun)
	{
		m_gravityGun.CollectLights(m_prepareDrawArgs.pointLights);
		m_gravityGun.Draw(m_renderCtx->meshBatch);
	}
	
	m_renderCtx->transparentMeshBatch.End(eg::DC);
	m_renderCtx->meshBatch.End(eg::DC);
	
	cpuTimerPrepare.Stop();
	
	m_particleManager.Step(dt, frustum, m_player.Rotation() * glm::vec3(0, 0, -1));
	
	auto cpuTimerPlanarRefl = eg::StartCPUTimer("Planar Reflections");
	auto gpuTimerPlanarRefl = eg::StartGPUTimer("Planar Reflections");
	
	m_planarReflectionsManager.BeginFrame();
	for (ReflectionPlane* reflectionPlane : m_prepareDrawArgs.reflectionPlanes)
	{
		m_planarReflectionsManager.RenderPlanarReflections(*reflectionPlane,
			[&] (const ReflectionPlane& plane, eg::FramebufferRef framebuffer)
		{
			RenderPlanarReflections(plane, framebuffer);
		});
	}
	
	cpuTimerPlanarRefl.Stop();
	gpuTimerPlanarRefl.Stop();
	
	m_plShadowMapper.UpdateShadowMaps(m_prepareDrawArgs.pointLights, [this] (const PointLightShadowRenderArgs& args)
	{
		RenderPointLightShadows(args);
	});
	
	DoDeferredRendering(true, *m_renderTarget);
	
	if (*physicsDebug)
	{
		m_physicsDebugRenderer.Render(m_physicsEngine, viewProjMatrix,
			m_renderOutputTexture, m_renderTarget->DepthTexture());
	}
	
	m_renderOutputTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	if (m_bloomRenderTarget != nullptr)
	{
		auto gpuTimerBloom = eg::StartGPUTimer("Bloom");
		auto cpuTimerBloom = eg::StartCPUTimer("Bloom");
		m_bloomRenderer->Render(glm::vec3(1.5f), m_renderOutputTexture, *m_bloomRenderTarget);
	}
	
	{
		auto gpuTimerPost = eg::StartGPUTimer("Post");
		auto cpuTimerPost = eg::StartCPUTimer("Post");
		m_postProcessor.Render(m_renderOutputTexture, m_bloomRenderTarget.get());
	}
	
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
	ImGui::Text("Reflection Planes: %d", (int)m_prepareDrawArgs.reflectionPlanes.size());
	
	ImGui::End();
	ImGui::PopStyleVar();
}
#endif

void MainGameState::SetResolution(int width, int height)
{
	m_projection.SetResolution(width, height);
	m_planarReflectionsManager.ResolutionChanged();
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
