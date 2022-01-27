#include "PausedMenu.hpp"
#include "OptionsMenu.hpp"
#include "../GameState.hpp"
#include "../MainMenuGameState.hpp"
#include "../Settings.hpp"
#include "../Editor/Editor.hpp"

constexpr float BUTTON_W = 200;

PausedMenu::PausedMenu()
	: m_widgetList(BUTTON_W)
{
	m_widgetList.AddWidget(Button("Resume", [&]
	{
		isPaused = false;
		optionsMenuOpen = false;
	}));
	m_widgetList.AddWidget(Button("Restart Level", [&]
	{
		shouldRestartLevel = true;
		isPaused = false;
		optionsMenuOpen = false;
	}));
	m_widgetList.AddWidget(Button("Options", [&] { optionsMenuOpen = true; }));
	m_mainMenuWidgetIndex = m_widgetList.AddWidget(Button("", [&]
	{
#ifndef IOMOMI_NO_EDITOR
		if (isFromEditor)
		{
			SetCurrentGS(editor);
			return;
		}
#endif
		mainMenuGameState->GoToMainScreen();
		SetCurrentGS(mainMenuGameState);
	}));
	
	m_widgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
}

constexpr float FADE_IN_TIME = 0.2f;

void PausedMenu::Update(float dt)
{
	shouldRestartLevel = false;
	
	if (settings.keyMenu.IsDown() && !settings.keyMenu.WasDown() && !KeyBindingWidget::anyKeyBindingPickingKey)
	{
		isPaused = !isPaused;
		optionsMenuOpen = false;
	}
	
	float fadeTarget = 0;
	float optionsFadeTarget = 0;
	
	if (isPaused)
	{
		fadeTarget = 1;
		optionsFadeTarget = optionsMenuOpen ? 1 : 0;
		
		if (m_optionsMenuOverlayFade > 0)
		{
			UpdateOptionsMenu(dt, glm::vec2(0), optionsMenuOpen);
		}
		
		std::get<Button>(m_widgetList.GetWidget(m_mainMenuWidgetIndex)).text =
			isFromEditor ? "Return to Editor" : "Main Menu";
		m_widgetList.position = glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY()) / 2.0f;
		m_widgetList.Update(dt, !optionsMenuOpen);
	}
	
	fade = eg::AnimateTo(fade, fadeTarget, dt / FADE_IN_TIME);
	m_optionsMenuOverlayFade = eg::AnimateTo(m_optionsMenuOverlayFade, optionsFadeTarget, dt / FADE_IN_TIME);
}

void PausedMenu::Draw(eg::SpriteBatch& spriteBatch) const
{
	if (!isPaused)
		return;
	
	if (m_optionsMenuOverlayFade > 0)
	{
		spriteBatch.DrawRect(
			eg::Rectangle(0, 0, eg::CurrentResolutionX(), eg::CurrentResolutionY()),
			eg::ColorLin(0, 0, 0, 0.3f * m_optionsMenuOverlayFade));
	}
	
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
