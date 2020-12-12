#include "ToggleButton.hpp"
#include "../GuiCommon.hpp"

static constexpr float FONT_SCALE = 0.6f;

void ToggleButton::Update(float dt, bool allowInteraction)
{
	UpdateBase(allowInteraction);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, m_hovered);
	
	if (m_hovered && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		setValue(!getValue());
	}
}

void ToggleButton::Draw(eg::SpriteBatch& spriteBatch) const
{
	DrawBase(spriteBatch, m_highlightIntensity, getValue() ? trueString : falseString);
}
