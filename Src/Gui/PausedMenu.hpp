#pragma once

#include "Widgets/WidgetList.hpp"
#include "GuiCommon.hpp"

class PausedMenu
{
public:
	PausedMenu();

	void Update(float dt);
	void Draw();

	bool isPaused = false;
	bool shouldRestartLevel = false;
	float fade = 0;
	bool isFromEditor = false;

private:
	GuiFrameArgs m_currentFrameArgs;
	
	eg::SpriteBatch m_spriteBatch;

	int m_buttonKeyboardFocus = -1;
	WidgetList m_widgetList;

	size_t m_mainMenuWidgetIndex;

	float m_optionsMenuOverlayFade = 0;
};
