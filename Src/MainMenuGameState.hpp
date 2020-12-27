#pragma once

#include "GameState.hpp"
#include "Gui/Widgets/WidgetList.hpp"
#include "World/PhysicsEngine.hpp"
#include "World/World.hpp"
#include "Graphics/BlurRenderer.hpp"

class MainMenuGameState : public GameState
{
public:
	MainMenuGameState();
	
	void RunFrame(float dt) override;
	
	void GoToMainScreen()
	{
		m_screen = Screen::Main;
	}
	
	void OnDeactivate() override;
	
	const struct Level* backgroundLevel = nullptr;
	
private:
	void ContinueClicked();
	
	void RenderWorld(float dt);
	
	void DrawLevelSelect(float dt, float xOffset);
	
	enum class Screen
	{
		Main,
		Options,
		LevelSelect
	};
	
	Screen m_screen = Screen::Main;
	
	eg::SpriteBatch m_spriteBatch;
	
	WidgetList m_mainWidgetList;
	
	bool m_transitionToMainScreen = true;
	float m_transitionProgress = 0;
	
	std::vector<float> m_levelHighlightIntensity;
	float m_levelSelectScroll = 0;
	float m_levelSelectScrollVel = 0;
	
	size_t m_numMainLevels = 0;
	std::vector<int64_t> m_levelIds;
	
	Button m_levelSelectBackButton;
	
	PhysicsEngine m_physicsEngine;
	std::unique_ptr<World> m_world;
	BlurRenderer m_worldBlurRenderer;
	eg::Framebuffer m_worldRenderFramebuffer;
	eg::Texture m_worldRenderTexture;
	float m_worldGameTime = 0;
	
	uint64_t m_lastFrameIndex = 0;
};

extern MainMenuGameState* mainMenuGameState;
