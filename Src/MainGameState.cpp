#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/RenderSettings.hpp"
#include "World/Entities/Entrance.hpp"
#include "Graphics/PlanarReflectionsManager.hpp"
#include "World/Entities/ECActivationLightStrip.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "Settings.hpp"
#include "Graphics/Materials/GravitySwitchVolLightMaterial.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = false;
	m_prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	
	eg::console::AddCommand("relms", 0, [this] (eg::Span<const std::string_view> args)
	{
		m_relativeMouseMode = !m_relativeMouseMode;
	});
	
	eg::console::AddCommand("exposure", 1, [this] (eg::Span<const std::string_view> args)
	{
		m_postProcessor.exposure = std::stof(std::string(args[0]));
	});
	
	eg::console::AddCommand("bloomIntensity", 1, [this] (eg::Span<const std::string_view> args)
	{
		m_postProcessor.bloomIntensity = std::stof(std::string(args[0]));
	});
}

void MainGameState::LoadWorld(std::istream& stream)
{
	m_world = World::Load(stream, false);
	m_player = { };
	m_gameTime = 0;
	
	for (eg::Entity& entity : m_world->EntityManager().GetEntitySet(ECEntrance::EntitySignature))
	{
		if (entity.GetComponent<ECEntrance>().GetType() == ECEntrance::Type::Entrance)
		{
			ECEntrance::InitPlayer(entity, m_player);
			break;
		}
	}
	
	m_world->InitializeBulletPhysics();
	
	ECActivationLightStrip::GenerateAll(*m_world);
	
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
	}
	
	{
		auto gpuTimerVolLight = eg::StartGPUTimer("Volumetric Lighting");
		auto cpuTimerVolLight = eg::StartCPUTimer("Volumetric Lighting");
		
		mDrawArgs.drawMode = MeshDrawMode::VolLight;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	}
	
	{
		auto gpuTimerLight = eg::StartGPUTimer("Lighting");
		auto cpuTimerLight = eg::StartCPUTimer("Lighting");
		
		m_renderCtx->renderer.BeginLighting(renderTarget, nullptr);
		
		m_renderCtx->renderer.DrawSpotLights(renderTarget, m_prepareDrawArgs.spotLights);
		m_renderCtx->renderer.DrawPointLights(renderTarget, m_prepareDrawArgs.pointLights);
		
		m_renderCtx->renderer.End(renderTarget);
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
	if (!eg::console::IsShown())
	{
		eg::SetRelativeMouseMode(m_relativeMouseMode);
		m_player.Update(*m_world, dt);
		
		WorldUpdateArgs updateArgs;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { m_plShadowMapper.Invalidate(sphere); };
		m_world->Update(updateArgs);
	}
	else
	{
		eg::SetRelativeMouseMode(false);
	}
	
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
	}
	
	GravityCornerLightMaterial::instance.Update(dt);
	
	if (m_renderTarget == nullptr || m_renderTarget->Width() != (uint32_t)eg::CurrentResolutionX() ||
		m_renderTarget->Height() != (uint32_t)eg::CurrentResolutionY())
	{
		eg::Texture2DCreateInfo textureCI;
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
			(uint32_t)eg::CurrentResolutionY(), m_renderOutputTexture, 0);
		
		if (settings.BloomEnabled())
		{
			m_bloomRenderTarget = std::make_unique<eg::BloomRenderer::RenderTarget>(
				(uint32_t)eg::CurrentResolutionX(), (uint32_t)eg::CurrentResolutionY(), 3);
		}
	}
	
	auto cpuTimerPrepare = eg::StartCPUTimer("Prepare Draw");
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	RenderSettings::instance->gameTime = m_gameTime;
	RenderSettings::instance->cameraPosition = m_player.EyePosition();
	RenderSettings::instance->viewProjection = m_projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	RenderSettings::instance->UpdateBuffer();
	
	m_renderCtx->meshBatch.Begin();
	
	m_prepareDrawArgs.spotLights.clear();
	m_prepareDrawArgs.pointLights.clear();
	m_prepareDrawArgs.reflectionPlanes.clear();
	m_world->PrepareForDraw(m_prepareDrawArgs);
	
	m_renderCtx->meshBatch.End(eg::DC);
	
	cpuTimerPrepare.Stop();
	
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
	
	m_lightProbesManager.PrepareForDraw(m_player.EyePosition());
	
	DoDeferredRendering(true, *m_renderTarget);
	
	m_renderOutputTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	if (m_bloomRenderTarget != nullptr)
	{
		auto gpuTimerBloom = eg::StartGPUTimer("Bloom");
		auto cpuTimerBloom = eg::StartCPUTimer("Bloom");
		m_bloomRenderer.Render(glm::vec3(1.0f), m_renderOutputTexture, *m_bloomRenderTarget);
	}
	
	{
		auto gpuTimerPost = eg::StartGPUTimer("Post");
		auto cpuTimerPost = eg::StartCPUTimer("Post");
		m_postProcessor.Render(m_renderOutputTexture, m_bloomRenderTarget.get());
	}
	
	DrawOverlay(dt);
	
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
	
	ImGui::End();
	ImGui::PopStyleVar();
}

void MainGameState::SetResolution(int width, int height)
{
	m_projection.SetResolution(width, height);
	m_planarReflectionsManager.ResolutionChanged();
}
