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
	
	eg::DC.ClearColor(0, eg::Color(0, 0, 0));
	eg::DC.ClearDepth(1.0f);
	
	eg::DC.SetViewport(0, 0, eg::CurrentResolutionX(), eg::CurrentResolutionY());
	
	m_renderSettings.viewProjection = m_projMatrix * viewMatrix;
	m_renderSettings.UpdateBuffer();
	eg::DC.BindUniformBuffer(m_renderSettings.Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	
	m_world.Draw(eg::CFrameIdx(), m_wallShader);
	
	m_imGuiInterface.EndFrame(eg::CFrameIdx());
}
