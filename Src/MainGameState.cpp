#include "MainGameState.hpp"
#include "World/PrepareDrawArgs.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	m_prepareDrawArgs.isEditor = false;
	m_prepareDrawArgs.objectRenderer = &m_renderCtx->objectRenderer;
	
	m_projection.SetFieldOfViewDeg(80.0f);
	
	/*
	m_spotLights[0].SetPosition(glm::vec3(2.5f, 4.0f, 1.5f));
	m_spotLights[0].SetDirection(glm::vec3(0, -1, 0.1f));
	m_spotLights[0].SetRadiance(eg::ColorLin(eg::ColorSRGB::FromHex(0xe4f1fc)).ScaleRGB(30));
	m_spotLights[0].SetCutoff(glm::radians(80.0f), 0.7f);
	
	m_spotLights[1].SetPosition(glm::vec3(2.5f, 4.0f, -4.5f));
	m_spotLights[1].SetDirection(glm::vec3(0, -1, -0.1f));
	m_spotLights[1].SetRadiance(eg::ColorLin(eg::ColorSRGB::FromHex(0xe4f1fc)).ScaleRGB(30));
	m_spotLights[1].SetCutoff(glm::radians(80.0f), 0.7f);*/
}

void MainGameState::LoadWorld(std::istream& stream)
{
	m_world.Load(stream);
	m_player = { };
	m_gameTime = 0;
}

void MainGameState::RunFrame(float dt)
{
	if (!eg::console::IsShown())
	{
		eg::SetRelativeMouseMode(true);
		m_player.Update(m_world, dt);
	}
	else
	{
		eg::SetRelativeMouseMode(false);
	}
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	RenderSettings::instance->gameTime = m_gameTime;
	RenderSettings::instance->cameraPosition = m_player.EyePosition();
	RenderSettings::instance->viewProjection = m_projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	RenderSettings::instance->UpdateBuffer();
	
	m_renderCtx->objectRenderer.Begin(ObjectMaterial::PipelineType::Game);
	
	m_prepareDrawArgs.spotLights.clear();
	m_world.PrepareForDraw(m_prepareDrawArgs);
	
	m_renderCtx->objectRenderer.End();
	
	m_renderCtx->renderer.BeginGeometry();
	
	m_world.Draw();
	
	m_renderCtx->objectRenderer.Draw();
	
	m_renderCtx->renderer.BeginLighting();
	
	m_renderCtx->renderer.DrawSpotLights(m_prepareDrawArgs.spotLights);
	
	m_renderCtx->renderer.End();
	
	DrawOverlay(dt);
	
	m_gameTime += dt;
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
