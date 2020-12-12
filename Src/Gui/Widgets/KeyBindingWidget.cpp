#include "KeyBindingWidget.hpp"
#include "../GuiCommon.hpp"

bool KeyBindingWidget::isConfiguringGamePad = false;
bool KeyBindingWidget::anyKeyBindingPickingKey = false;

KeyBindingWidget::KeyBindingWidget(std::string _label, KeyBinding& binding)
	: m_binding(&binding)
{
	label = std::move(_label);
}

void KeyBindingWidget::Update(float dt, bool allowInteraction)
{
	UpdateBase(allowInteraction);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, m_hovered || m_isPickingKey);
	
	if (m_isPickingKey)
	{
		anyKeyBindingPickingKey = true;
		eg::Button button = eg::InputState::Current().PressedButton();
		if (button != eg::Button::Unknown && eg::InputState::Previous().PressedButton() != button)
		{
			(isConfiguringGamePad ? m_binding->controllerButton : m_binding->kbmButton) = button;
			m_isPickingKey = false;
		}
	}
	else if (m_hovered && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		m_isPickingKey = true;
	}
}

void KeyBindingWidget::Draw(eg::SpriteBatch& spriteBatch) const
{
	std::string_view valueString;
	if (m_isPickingKey)
	{
		valueString = "Press any button";
	}
	else
	{
		eg::Button currentButton = isConfiguringGamePad ? m_binding->controllerButton : m_binding->kbmButton;
		valueString = eg::ButtonDisplayName(currentButton);
	}
	
	DrawBase(spriteBatch, m_highlightIntensity, valueString);
}
