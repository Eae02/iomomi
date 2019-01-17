#include "Game.hpp"

#include "Graphics/Graphics.hpp"

Game::Game()
{
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			for (int z = 0; z < 3; z++)
			{
				m_world.SetIsAir({ x, y, z }, true);
			}
		}
	}
}

void Game::RunFrame(float dt)
{
	m_imGuiInterface.NewFrame();
	
	m_player.Update(m_world, dt);
	
	m_projMatrix = glm::perspectiveFov<float>(glm::radians(70.0f), eg::CurrentResolutionX(), eg::CurrentResolutionY(), 0.1f, 1000.0f);
	glm::mat4 viewMatrix, inverseViewMatrix;
	m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
	
	m_renderSettings.viewProjection = m_projMatrix * viewMatrix;
	m_renderSettings.UpdateBuffer();
	
	m_world.PrepareForDraw();
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.depthClearValue = 1.0f;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::Color(0.2f, 1.0f, 1.0f);
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	m_world.Draw(m_renderSettings, m_wallShader);
	
	eg::DC.EndRenderPass();
	
	ImGui::ShowDemoWindow();
	
	m_imGuiInterface.EndFrame();
}
