#pragma once

#include "Widgets/WidgetList.hpp"

class PausedMenu
{
public:
	PausedMenu();
	
	void Update(float dt);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	bool isPaused = false;
	bool shouldRestartLevel = false;
	float fade = 0;
	bool isFromEditor = false;
	
private:
	int m_buttonKeyboardFocus = -1;
	WidgetList m_widgetList;
	
	size_t m_mainMenuWidgetIndex;
	
	float m_optionsMenuOverlayFade = 0;
};
