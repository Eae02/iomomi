#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/RenderSettings.hpp"
#include "World/Entities/Entrance.hpp"
#include "World/Entities/ECActivationLightStrip.hpp"
#include "World/Entities/ECPlatform.hpp"
#include "Graphics/PlanarReflectionsManager.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "Graphics/Materials/GravitySwitchVolLightMaterial.hpp"
#include "Settings.hpp"
#include "Levels.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = false;
	m_prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	m_prepareDrawArgs.transparentMeshBatch = &m_renderCtx->transparentMeshBatch;
	m_prepareDrawArgs.player = &m_player;
	m_projection.SetZNear(0.02f);
	m_projection.SetZFar(200.0f);
	
	eg::console::AddCommand("relms", 0, [this] (eg::Span<const std::string_view> args)
	{
		m_relativeMouseMode = !m_relativeMouseMode;
	});
	
	eg::console::AddCommand("bloomIntensity", 1, [this] (eg::Span<const std::string_view> args)
	{
		m_postProcessor.bloomIntensity = std::stof(std::string(args[0]));
	});
	
	eg::console::AddCommand("reload", 0, [this] (eg::Span<const std::string_view> args)
	{
		if (m_currentLevelIndex == -1)
		{
			eg::Log(eg::LogLevel::Error, "lvl", "No level to reload");
			return;
		}
		
		std::string levelPath = GetLevelPath(levels[m_currentLevelIndex].name);
		std::ifstream levelStream(levelPath, std::ios::binary);
		LoadWorld(levelStream, m_currentLevelIndex);
		eg::console::Hide();
	});
	
	m_particleManager.SetTextureSize(1024, 1024);
}

void MainGameState::LoadWorld(std::istream& stream, int64_t levelIndex, const eg::Entity* exitEntity)
{
	auto newWorld = World::Load(stream, false);
	m_player.Reset();
	m_gameTime = 0;
	m_currentLevelIndex = levelIndex;
	
	for (eg::Entity& entity : newWorld->EntityManager().GetEntitySet(ECEntrance::EntitySignature))
	{
		if (entity.GetComponent<ECEntrance>().GetType() == ECEntrance::Type::Entrance)
		{
			if (exitEntity != nullptr)
			{
				ECEntrance::MovePlayer(*exitEntity, entity, m_player);
			}
			else
			{
				ECEntrance::InitPlayer(entity, m_player);
			}
			break;
		}
	}
	
	m_world = std::move(newWorld);
	
	m_world->InitializeBulletPhysics();
	
	ECActivationLightStrip::GenerateAll(*m_world);
	
	m_waterSimulator.Init(*m_world);
	
	m_plShadowMapper.InvalidateAll();
}

void MainGameState::DoDeferredRendering(bool useLightProbes, DeferredRenderer::RenderTarget& renderTarget)
{
	MeshDrawArgs mDrawArgs;
	
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
		
		m_renderCtx->renderer.BeginEmissive(renderTarget);
		
		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
	}
	
	{
		auto gpuTimerLight = eg::StartGPUTimer("Lighting");
		auto cpuTimerLight = eg::StartCPUTimer("Lighting");
		
		m_renderCtx->renderer.BeginLighting(renderTarget);
		
		m_renderCtx->renderer.DrawReflectionPlaneLighting(renderTarget, m_prepareDrawArgs.reflectionPlanes);
		m_renderCtx->renderer.DrawSpotLights(renderTarget, m_prepareDrawArgs.spotLights);
		m_renderCtx->renderer.DrawPointLights(renderTarget, m_prepareDrawArgs.pointLights);
		//m_renderCtx->renderer.DrawWaterBasic(m_waterSimulator.GetPositionsBuffer(), m_waterSimulator.NumParticles());
		
		m_particleRenderer.Draw(m_particleManager, renderTarget.ResolvedDepthTexture());
		
		m_renderCtx->renderer.End(renderTarget);
	}
	
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
	
	if (!eg::console::IsShown() && !settingsWindowVisible)
	{
		auto worldUpdateCPUTimer = eg::StartCPUTimer("World Update");
		
		WorldUpdateArgs updateArgs;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { m_plShadowMapper.Invalidate(sphere); };
		
		ECPlatform::Update(updateArgs);
		
		eg::SetRelativeMouseMode(m_relativeMouseMode);
		m_player.Update(*m_world, dt);
		
		UpdateViewProjMatrices();
		if (m_world->playerHasGravityGun)
		{
			m_gravityGun.Update(*m_world, m_particleManager, m_player, inverseViewProjMatrix, dt);
		}
		
		eg::ECParticleSystem::Update(m_world->EntityManager());
		
		eg::Entity* currentExit = nullptr;
		ECEntrance::Update(updateArgs, &currentExit);
		
		if (currentExit != nullptr && m_currentLevelIndex != -1)
		{
			const std::string& exitName = currentExit->GetComponent<ECEntrance>().ExitName();
			int64_t nextLevelIndex = GetNextLevelIndex(m_currentLevelIndex, exitName);
			if (nextLevelIndex != -1)
			{
				eg::Log(eg::LogLevel::Info, "lvl", "Going to next level '{0}'", levels[nextLevelIndex].name);
				std::string path = GetLevelPath(levels[nextLevelIndex].name);
				std::ifstream stream(path, std::ios::binary);
				LoadWorld(stream, nextLevelIndex, currentExit);
			}
		}
		
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		m_world->Update(updateArgs);
	}
	else
	{
		eg::SetRelativeMouseMode(false);
		UpdateViewProjMatrices();
	}
	
	eg::Frustum frustum(inverseViewProjMatrix);
	
	if (m_lastSettingsGeneration != SettingsGeneration())
	{
		m_projection.SetFieldOfViewDeg(settings.fieldOfViewDeg);
		
		m_planarReflectionsManager.SetQuality(settings.reflectionsQuality);
		m_plShadowMapper.SetQuality(settings.shadowQuality);
		
		GravitySwitchVolLightMaterial::SetQuality(settings.lightingQuality);
		
		m_lastSettingsGeneration = SettingsGeneration();
		
		if (m_lightingQuality != settings.lightingQuality || m_msaaSamples != settings.msaaSamples)
		{
			m_msaaSamples = settings.msaaSamples;
			m_lightingQuality = settings.lightingQuality;
			m_renderTarget.reset();
			m_bloomRenderTarget.reset();
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
			(uint32_t)eg::CurrentResolutionY(), settings.msaaSamples, m_renderOutputTexture, 0);
		
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
	
	m_waterSimulator.Update(dt);
	
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
	}, true);
	
	//m_lightProbesManager.PrepareForDraw(m_player.EyePosition());
	
	DoDeferredRendering(true, *m_renderTarget);
	
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
	
	if (eg::DevMode())
	{
		DrawOverlay(dt);
	}
	
	m_gameTime += dt;
}

void MainGameState::RenderPointLightShadows(const PointLightShadowRenderArgs& args)
{
	m_world->DrawPointLightShadows(args);
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.drawMode = MeshDrawMode::PointLightShadow;
	mDrawArgs.plShadowRenderArgs = &args;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
}

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
	m_player.DebugDraw();
	ImGui::Text("Particles: %d", m_particleManager.ParticlesToDraw());
	ImGui::Text("Water Spheres: %d", m_waterSimulator.NumParticles());
	ImGui::Text("Reflection Planes: %d", (int)m_prepareDrawArgs.reflectionPlanes.size());
	ImGui::Text("Underwater: %f", m_renderCtx->renderer.FragmentsUnderwater());
	
	ImGui::End();
	ImGui::PopStyleVar();
}

void MainGameState::SetResolution(int width, int height)
{
	m_projection.SetResolution(width, height);
	m_planarReflectionsManager.ResolutionChanged();
}
