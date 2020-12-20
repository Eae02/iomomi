#include "KeyBindingWidget.hpp"
#include "../GuiCommon.hpp"

bool KeyBindingWidget::isConfiguringGamePad = false;
bool KeyBindingWidget::anyKeyBindingPickingKey = false;

static const std::string_view cancelText = "Cancel";
static constexpr float CANCEL_BUTTON_WIDTH = 70;

KeyBindingWidget::KeyBindingWidget(std::string _label, KeyBinding& binding)
	: m_binding(&binding)
{
	label = std::move(_label);
}

void KeyBindingWidget::Update(float dt, bool allowInteraction)
{
	UpdateBase(allowInteraction);
	m_cancelRectangle = eg::Rectangle(m_rectangle.MaxX() + 10 * m_cancelButtonAlpha, m_rectangle.y, CANCEL_BUTTON_WIDTH, m_rectangle.h);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, m_hovered || m_isPickingKey);
	AnimateProperty(m_cancelButtonAlpha, dt, style::HoverAnimationTime, m_isPickingKey);
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	bool cancelButtonHovered = allowInteraction && m_cancelButtonAlpha > 0 && m_cancelRectangle.Contains(flippedCursorPos);
	AnimateProperty(m_cancelHighlightIntensity, dt, style::HoverAnimationTime, cancelButtonHovered);
	
	if (m_isPickingKey)
	{
		if (cancelButtonHovered && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
		{
			m_isPickingKey = false;
		}
		else
		{
			anyKeyBindingPickingKey = true;
			const eg::Button button = eg::InputState::Current().PressedButton();
			if (button != eg::Button::Unknown && eg::InputState::Previous().PressedButton() != button &&
				IsControllerButton(button) == isConfiguringGamePad)
			{
				(isConfiguringGamePad ? m_binding->controllerButton : m_binding->kbmButton) = button;
				m_isPickingKey = false;
			}
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
	
	if (m_cancelButtonAlpha > 0)
	{
		//Draws the cancel button's background rectangle
		eg::ColorLin backColor =
			eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, m_cancelHighlightIntensity)
			.ScaleAlpha(m_cancelButtonAlpha);
		spriteBatch.DrawRect(m_cancelRectangle, backColor);
		
		spriteBatch.DrawText(*style::UIFont, cancelText, GetTextPos(m_cancelRectangle), eg::ColorLin(1, 1, 1, m_cancelButtonAlpha),
		                     FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	}
}
