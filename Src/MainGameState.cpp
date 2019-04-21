#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/RenderSettings.hpp"
#include "World/Entities/Entrance.hpp"
#include "Graphics/PlanarReflectionsManager.hpp"
#include "World/Entities/ECActivationLightStrip.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = false;
	m_prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	
	m_projection.SetFieldOfViewDeg(80.0f);
	
	eg::console::AddCommand("relms", 0, [this] (eg::Span<const std::string_view> args) {
		m_relativeMouseMode = !m_relativeMouseMode;
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
	m_renderCtx->renderer.BeginGeometry(renderTarget);
	
	m_world->Draw();
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.drawMode = MeshDrawMode::Game;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_renderCtx->renderer.BeginEmissive(renderTarget);
	
	mDrawArgs.drawMode = MeshDrawMode::Emissive;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	mDrawArgs.drawMode = MeshDrawMode::VolLight;
	m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
	
	m_renderCtx->renderer.BeginLighting(renderTarget, nullptr);
	
	m_renderCtx->renderer.DrawSpotLights(renderTarget, m_prepareDrawArgs.spotLights);
	m_renderCtx->renderer.DrawPointLights(renderTarget, m_prepareDrawArgs.pointLights);
	
	m_renderCtx->renderer.End(renderTarget);
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
	
	GravityCornerLightMaterial::instance.Update(dt);
	
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
			(uint32_t)eg::CurrentResolutionX(), (uint32_t)eg::CurrentResolutionY(), 3);
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
	m_prepareDrawArgs.reflectionPlanes.clear();
	m_world->PrepareForDraw(m_prepareDrawArgs);
	
	m_renderCtx->meshBatch.End(eg::DC);
	
	m_planarReflectionsManager.BeginFrame();
	for (ReflectionPlane* reflectionPlane : m_prepareDrawArgs.reflectionPlanes)
	{
		m_planarReflectionsManager.RenderPlanarReflections(*reflectionPlane,
			[&] (const ReflectionPlane& plane, eg::FramebufferRef framebuffer)
		{
			RenderPlanarReflections(plane, framebuffer);
		});
	}
	
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
	m_planarReflectionsManager.ResolutionChanged();
}
