#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "World/Entities/EntranceEntity.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "World/Entities/LightProbeEntity.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = false;
	m_prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	
	m_projection.SetFieldOfViewDeg(80.0f);
}

void MainGameState::LoadWorld(std::istream& stream)
{
	m_world = World::Load(stream);
	m_player = { };
	m_gameTime = 0;
	
	for (const std::shared_ptr<Entity>& entity : m_world->Entities())
	{
		if (EntranceEntity* entrance = dynamic_cast<EntranceEntity*>(entity.get()))
		{
			if (entrance->EntranceType() == EntranceEntity::Type::Entrance)
			{
				entrance->InitPlayer(m_player);
				break;
			}
		}
	}
	
	m_world->InitializeBulletPhysics();
	
	m_plShadowMapper.InvalidateAll();
}

void MainGameState::DoDeferredRendering(bool useLightProbes, DeferredRenderer::RenderTarget& renderTarget)
{
	m_renderCtx->renderer.BeginGeometry(renderTarget);
	
	m_world->Draw();
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.drawMode = MeshDrawMode::Game;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_renderCtx->renderer.BeginEmissive(renderTarget);
	
	mDrawArgs.drawMode = MeshDrawMode::Emissive;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_renderCtx->renderer.BeginLighting(renderTarget, nullptr);
	
	m_renderCtx->renderer.DrawSpotLights(renderTarget, m_prepareDrawArgs.spotLights);
	m_renderCtx->renderer.DrawPointLights(renderTarget, m_prepareDrawArgs.pointLights);
	
	m_renderCtx->renderer.End(renderTarget);
}

void MainGameState::RunFrame(float dt)
{
	if (!eg::console::IsShown())
	{
		eg::SetRelativeMouseMode(!eg::DevMode());
		m_player.Update(*m_world, dt);
		
		Entity::UpdateArgs entityUpdateArgs;
		entityUpdateArgs.dt = dt;
		entityUpdateArgs.player = &m_player;
		entityUpdateArgs.world = m_world.get();
		entityUpdateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { m_plShadowMapper.Invalidate(sphere); };
		m_world->Update(entityUpdateArgs);
	}
	else
	{
		eg::SetRelativeMouseMode(false);
	}
	
	if (m_renderTarget == nullptr || m_renderTarget->Width() != (uint32_t)eg::CurrentResolutionX() ||
		m_renderTarget->Height() != (uint32_t)eg::CurrentResolutionY())
	{
		eg::Texture2DCreateInfo textureCI;
		textureCI.format = DeferredRenderer::LIGHT_COLOR_FORMAT;
		textureCI.width = (uint32_t)eg::CurrentResolutionX();
		textureCI.height = (uint32_t)eg::CurrentResolutionY();
		textureCI.mipLevels = 1;
		textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment;
		m_renderOutputTexture = eg::Texture::Create2D(textureCI);
		
		m_renderTarget = std::make_unique<DeferredRenderer::RenderTarget>((uint32_t)eg::CurrentResolutionX(),
			(uint32_t)eg::CurrentResolutionY(), m_renderOutputTexture, 0);
		
		m_bloomRenderTarget = std::make_unique<eg::BloomRenderer::RenderTarget>(
			(uint32_t)eg::CurrentResolutionX(), (uint32_t)eg::CurrentResolutionY());
	}
	
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
	m_world->PrepareForDraw(m_prepareDrawArgs);
	
	m_renderCtx->meshBatch.End(eg::DC);
	
	m_plShadowMapper.UpdateShadowMaps(m_prepareDrawArgs.pointLights, [this] (const PointLightShadowRenderArgs& args)
	{
		RenderPointLightShadows(args);
	}, 4);
	
	m_lightProbesManager.PrepareForDraw(m_player.EyePosition());
	
	DoDeferredRendering(true, *m_renderTarget);
	
	m_renderOutputTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	
	m_bloomRenderer.Render(glm::vec3(1.0f), m_renderOutputTexture, *m_bloomRenderTarget);
	
	m_postProcessor.Render(m_renderOutputTexture, m_bloomRenderTarget->OutputTexture());
	
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
}
