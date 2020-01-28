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
	
private:
	float m_fadeIn = 0;
	
	int m_buttonKeyboardFocus = -1;
	WidgetList m_widgetList;
	
	bool m_inOptionsMenu = false;
};
