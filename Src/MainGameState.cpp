#include "MainGameState.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

MainGameState::MainGameState(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx)
{
	std::ifstream stream(eg::ExeRelPath("levels/test.gwd"), std::ios::binary);
	m_world.Load(stream);
	stream.close();
}

void MainGameState::RunFrame(float dt)
{
	if (!eg::console::IsShown())
	{
		m_player.Update(m_world, dt);
	}
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	RenderSettings::instance->gameTime = m_gameTime;
	RenderSettings::instance->cameraPosition = m_player.EyePosition();
	RenderSettings::instance->viewProjection = m_projection.Matrix() * viewMatrix;
	RenderSettings::instance->invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	RenderSettings::instance->UpdateBuffer();
	
	m_renderCtx->objectRenderer.Begin(ObjectMaterial::PipelineType::Game);
	
	m_world.PrepareForDraw(m_renderCtx->objectRenderer, false);
	
	m_renderCtx->objectRenderer.End();
	
	m_renderCtx->renderer.Begin(nullptr);
	
	m_world.Draw();
	
	m_renderCtx->objectRenderer.Draw();
	
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
