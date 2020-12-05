#include "Button.hpp"
#include "../GuiCommon.hpp"

void Button::Update(float dt, bool allowInteraction)
{
	if (eg::FrameIdx() - m_lastUpdateFrameIdx > 1)
		m_highlightIntensity = 0;
	m_lastUpdateFrameIdx = eg::FrameIdx();
	
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
	float fontScale = 0.75f * (1 + inflate);
	
	eg::Rectangle rect(position, width, height);
	
	DrawBackground(spriteBatch, rect, m_highlightIntensity);
	
	glm::vec2 extents = glm::vec2(style::UIFont->GetTextExtents(text).x, style::UIFont->Size()) * fontScale;
	
	spriteBatch.DrawText(*style::UIFont, text, rect.Center() - extents / 2.0f, eg::ColorLin(eg::Color::White),
	                     fontScale, nullptr, eg::TextFlags::NoPixelAlign | eg::TextFlags::DropShadow);
}

void Button::DrawBackground(eg::SpriteBatch& spriteBatch, const eg::Rectangle& rect, float highlightIntensity)
{
	float inflate = highlightIntensity * style::ButtonInflatePercent;
	
	glm::vec2 inflateSize = inflate * rect.Size();
	eg::Rectangle inflatedRect(
		rect.x - inflateSize.x, rect.y - inflateSize.y, 
		rect.w + inflateSize.x * 2, rect.h + inflateSize.y * 2);
	
	eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, highlightIntensity);
	spriteBatch.DrawRect(inflatedRect, backColor);
}
