#include "PausedMenu.hpp"
#include "OptionsMenu.hpp"
#include "../GameState.hpp"
#include "../MainMenuGameState.hpp"

constexpr float BUTTON_W = 200;

PausedMenu::PausedMenu()
	: m_widgetList(BUTTON_W)
{
	m_widgetList.AddWidget(Button("Resume", [&] { isPaused = false; }));
	m_widgetList.AddWidget(Button("Restart Level", [&]
	{
		shouldRestartLevel = true;
		isPaused = false;
	}));
	m_widgetList.AddWidget(Button("Options", [&] { optionsMenuOpen = true; }));
	m_widgetList.AddWidget(Button("Main Menu", [&]
	{
		mainMenuGameState->GoToMainScreen();
		SetCurrentGS(mainMenuGameState);
	}));
	
	m_widgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
}

constexpr float FADE_IN_TIME = 0.2f;

void PausedMenu::Update(float dt)
{
	shouldRestartLevel = false;
	
	if (eg::IsButtonDown(eg::Button::Escape) && !eg::WasButtonDown(eg::Button::Escape))
		isPaused = !isPaused;
	
	if (!isPaused)
	{
		fade = std::max(fade - dt / FADE_IN_TIME, 0.0f);
		return;
	}
	
	fade = std::min(fade + dt / FADE_IN_TIME, 1.0f);
	/*
	auto ChangeKeyboardFocus = [&] (int delta)
	{
		m_buttons[m_buttonKeyboardFocus].hasKeyboardFocus = false;
		m_buttonKeyboardFocus = (m_buttonKeyboardFocus + delta + BTN_Count) % BTN_Count;
		m_buttons[m_buttonKeyboardFocus].hasKeyboardFocus = true;
	};
	
	if ((eg::IsButtonDown(eg::Button::UpArrow) && !eg::WasButtonDown(eg::Button::UpArrow)) ||
		(eg::IsButtonDown(eg::Button::CtrlrDPadUp) && !eg::WasButtonDown(eg::Button::CtrlrDPadUp)))
	{
		ChangeKeyboardFocus(-1);
	}
	if ((eg::IsButtonDown(eg::Button::DownArrow) && !eg::WasButtonDown(eg::Button::DownArrow)) ||
		(eg::IsButtonDown(eg::Button::CtrlrDPadDown) && !eg::WasButtonDown(eg::Button::CtrlrDPadDown)))
	{
		ChangeKeyboardFocus(1);
	}
	*/
	
	if (optionsMenuOpen)
	{
		UpdateOptionsMenu(dt);
	}
	
	m_widgetList.position = glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY()) / 2.0f;
	m_widgetList.Update(dt, !optionsMenuOpen);
}

void PausedMenu::Draw(eg::SpriteBatch& spriteBatch) const
{
	if (!isPaused)
		return;
	
	if (optionsMenuOpen)
	{
		DrawOptionsMenu(spriteBatch);
	}
	else
	{
		m_widgetList.Draw(spriteBatch);
	}
	
	if (ComboBox::current)
	{
		ComboBox::current->DrawOverlay(spriteBatch);
	}
}
