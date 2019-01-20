#pragma once

#include "ImGuiInterface.hpp"
#include "Graphics/RenderSettings.hpp"
#include "Graphics/WallShader.hpp"
#include "Graphics/ObjectRenderer.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"

class Game : public eg::IGame
{
public:
	Game();
	
	void RunFrame(float dt) override;
	
	void ResolutionChanged(int newWidth, int newHeight) override;
	
private:
	eg::PerspectiveProjection m_projection;
	float m_gameTime = 0;
	
	ImGuiInterface m_imGuiInterface;
	RenderSettings m_renderSettings;
	World m_world;
	Player m_player;
	WallShader m_wallShader;
	
	ObjectRenderer m_objectRenderer;
};
