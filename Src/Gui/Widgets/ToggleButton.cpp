#include "ToggleButton.hpp"
#include "ComboBox.hpp"
#include "../GuiCommon.hpp"

static constexpr float FONT_SCALE = 0.6f;

void ToggleButton::Update(float dt, bool allowInteraction)
{
	m_rectangle = eg::Rectangle(position.x + width / 2, position.y, width / 2, height);
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	bool hovered = allowInteraction && m_rectangle.Contains(flippedCursorPos);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, hovered);
	
	if (hovered && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		setValue(!getValue());
	}
}

void ToggleButton::Draw(eg::SpriteBatch& spriteBatch) const
{
	ComboBox::DrawButton(spriteBatch, m_rectangle, m_highlightIntensity, label, getValue() ? trueString : falseString);
}
