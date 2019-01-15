#pragma once

#include <EGame/EG.hpp>

#include "ImGuiInterface.hpp"
#include "Graphics/RenderSettings.hpp"
#include "Graphics/WallShader.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"

class Game : public eg::IGame
{
public:
	Game();
	
	void RunFrame(float dt) override;
	
private:
	glm::mat4 m_projMatrix;
	
	ImGuiInterface m_imGuiInterface;
	RenderSettings m_renderSettings;
	World m_world;
	Player m_player;
	WallShader m_wallShader;
};
