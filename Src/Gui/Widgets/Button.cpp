#include "Button.hpp"
#include "../GuiCommon.hpp"

void Button::Update(float dt, bool allowInteraction)
{
	if (std::abs(eg::CursorDeltaX()) > 2 || std::abs(eg::CursorDeltaY()) > 2)
		hasKeyboardFocus = false;
	
	eg::Rectangle rect(position, width, height);
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	bool hovered = allowInteraction && (rect.Contains(flippedCursorPos) || hasKeyboardFocus);
	bool clicked = hovered &&
		((eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft)) || 
			(hasKeyboardFocus && eg::IsButtonDown(eg::Button::Enter)));
	
	if (hovered)
		m_highlightIntensity = std::min(m_highlightIntensity + dt / style::HoverAnimationTime, 1.0f);
	else
		m_highlightIntensity = std::max(m_highlightIntensity - dt / style::HoverAnimationTime, 0.0f);
	
	if (clicked && onClick)
		onClick();
}

void Button::Draw(eg::SpriteBatch& spriteBatch) const
{
	float inflate = m_highlightIntensity * style::ButtonInflatePercent;
	float FONT_SCALE = 0.75f * (1 + inflate);
	
	eg::Rectangle rect(position, width, height);
	
	glm::vec2 inflateSize = inflate * glm::vec2(width, height);
	eg::Rectangle inflatedRect(
		rect.x - inflateSize.x, rect.y - inflateSize.y, 
		rect.w + inflateSize.x * 2, rect.h + inflateSize.y * 2);
	
	eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, m_highlightIntensity);
	spriteBatch.DrawRect(inflatedRect, backColor);
	
	glm::vec2 extents = glm::vec2(style::UIFont->GetTextExtents(text).x, style::UIFont->Size()) * FONT_SCALE;
	
	spriteBatch.DrawText(*style::UIFont, text, inflatedRect.Center() - extents / 2.0f, eg::ColorLin(eg::Color::White),
		FONT_SCALE, nullptr, eg::TextFlags::NoPixelAlign);
}
