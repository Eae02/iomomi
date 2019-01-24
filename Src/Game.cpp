#include "Game.hpp"
#include "GameState.hpp"
#include "Editor/Editor.hpp"
#include "Graphics/Materials/GravityCornerMaterial.hpp"

#include <fstream>

Game::Game()
{
	editor = std::make_unique<Editor>();
	currentGS = editor.get();
	
	for (int x = 0; x < 3; x++)
	{
		for (int y = -1; y < 3; y++)
		{
			for (int z = 0; z < 3; z++)
			{
				m_world.SetIsAir({ x, y, z }, true);
			}
		}
	}
	
	m_world.SetIsAir({ 3, 0, 0 }, true);
	m_world.SetIsAir({ 3, 0, 1 }, true);
	m_world.SetIsAir({ 3, 0, 2 }, true);
	m_world.SetIsAir({ 3, 1, 0 }, true);
	m_world.SetIsAir({ 3, 1, 1 }, true);
	m_world.SetIsAir({ 3, 1, 2 }, true);
	m_world.SetIsAir({ 4, 0, 0 }, true);
	m_world.SetIsAir({ 4, 0, 1 }, true);
	m_world.SetIsAir({ 4, 0, 2 }, true);
	m_world.SetIsAir({ 4, 1, 0 }, true);
	m_world.SetIsAir({ 4, 1, 1 }, true);
	m_world.SetIsAir({ 4, 1, 2 }, true);
	
	m_world.SetIsAir({ -1, 0, 0 }, true);
	m_world.SetIsAir({ -1, 0, 1 }, true);
	m_world.SetIsAir({ -1, 1, 0 }, true);
	m_world.SetIsAir({ -1, 1, 1 }, true);
	m_world.SetIsAir({ -2, 0, 0 }, true);
	m_world.SetIsAir({ -2, 0, 1 }, true);
	m_world.SetIsAir({ -2, 1, 0 }, true);
	m_world.SetIsAir({ -2, 1, 1 }, true);
	
	m_world.AddGravityCorner({ Dir::NegY, Dir::NegZ, glm::vec3(0, -1, 0), 3 });
	m_world.AddGravityCorner({ Dir::NegZ, Dir::PosX, glm::vec3(5, 3, 0), 3 });
	m_world.AddGravityCorner({ Dir::PosX, Dir::PosZ, glm::vec3(5, 3, 3), 3 });
	m_world.AddGravityCorner({ Dir::PosZ, Dir::PosY, glm::vec3(0, 2, 2), 2 });
	
	std::ofstream stream(eg::ExeRelPath("world"), std::ios::binary);
	m_world.Save(stream);
	stream.close();
	
	/*
	std::ifstream stream(eg::ExeRelPath("world"), std::ios::binary);
	m_world.Load(stream);
	stream.close();*/
	
	m_projection.SetFieldOfViewDeg(80.0f);
}

void Game::ResolutionChanged(int newWidth, int newHeight)
{
	m_projection.SetResolution(newWidth, newHeight);
}

void Game::RunFrame(float dt)
{
	m_imGuiInterface.NewFrame();
	
	currentGS->RunFrame(dt);
	/*
	if (!eg::console::IsShown())
	{
		m_player.Update(m_world, dt);
	}
	
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	m_renderSettings.viewProjection = m_projection.Matrix() * viewMatrix;
	m_renderSettings.invViewProjection = inverseViewMatrix * m_projection.InverseMatrix();
	m_renderSettings.gameTime = m_gameTime;
	m_renderSettings.UpdateBuffer();
	
	m_objectRenderer.Begin();
	
	m_world.PrepareForDraw(m_objectRenderer);
	
	m_objectRenderer.End();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Discard;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world.Draw(m_renderSettings, m_wallShader);
	
	m_objectRenderer.Draw(m_renderSettings);
	
	eg::DC.EndRenderPass();
	*/
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
	
	m_imGuiInterface.EndFrame();
	
	m_gameTime += dt;
}
