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
	
	m_renderCtx->renderSettings.viewProjection = m_renderCtx->projection.Matrix() * viewMatrix;
	m_renderCtx->renderSettings.invViewProjection = inverseViewMatrix * m_renderCtx->projection.InverseMatrix();
	m_renderCtx->renderSettings.UpdateBuffer();
	
	m_renderCtx->objectRenderer.Begin();
	
	m_world.PrepareForDraw(m_renderCtx->objectRenderer);
	
	m_renderCtx->objectRenderer.End();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world.Draw(m_renderCtx->renderSettings);
	
	m_renderCtx->objectRenderer.Draw(m_renderCtx->renderSettings);
	
	eg::DC.EndRenderPass();
	
	DrawOverlay(dt);
}

void MainGameState::DrawOverlay(float dt)
{
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(150, 0), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
	ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);
	
	const char* graphicsAPIName;
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL)
		graphicsAPIName = "OpenGL";
	else if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan)
		graphicsAPIName = "Vulkan";
	
	ImGui::Text("FPS: %.3f", (1.0f / dt));
	ImGui::Text("Frame time: %.3fms", dt * 1000.0f);
	ImGui::Text("Graphics API: %s", graphicsAPIName);
	m_player.DebugDraw();
	
	ImGui::End();
	ImGui::PopStyleVar();
}
