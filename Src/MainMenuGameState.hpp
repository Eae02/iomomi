#pragma once

#include "GameState.hpp"
#include "Gui/Widgets/WidgetList.hpp"

class MainMenuGameState : public GameState
{
public:
	MainMenuGameState();
	
	void RunFrame(float dt) override;
	
	void GoToMainScreen()
	{
		m_screen = Screen::Main;
	}
	
private:
	void DrawLevelSelect(float dt);
	
	enum class Screen
	{
		Main,
		Options,
		LevelSelect
	};
	
	Screen m_screen = Screen::Main;
	
	WidgetList m_mainWidgetList;
	
	std::vector<float> m_levelHighlightIntensity;
	int m_levelSelectScroll = 0;
	bool m_inExtraLevelsTab = false;
	
	std::vector<int64_t> m_mainLevelIds;
	std::vector<int64_t> m_extraLevelIds;
	
	Button m_levelSelectBackButton;
};

extern MainMenuGameState* mainMenuGameState;
