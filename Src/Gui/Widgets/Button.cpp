#include "Button.hpp"

#include "../GuiCommon.hpp"

void Button::Update(const GuiFrameArgs& frameArgs, bool allowInteraction)
{
	if (m_lastUpdateFrameIdx + 1 != eg::FrameIdx())
	{
		m_highlightIntensity = 0;
		m_wasHovered = false;
	}
	m_lastUpdateFrameIdx = eg::FrameIdx();

	if (std::abs(eg::CursorDeltaX()) > 2 || std::abs(eg::CursorDeltaY()) > 2)
		hasKeyboardFocus = false;

	eg::Rectangle rect(position, width, height);

	bool hovered = enabled && allowInteraction && (rect.Contains(frameArgs.cursorPos) || hasKeyboardFocus);
	bool clicked = hovered && ((eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft)) ||
	                           (hasKeyboardFocus && eg::IsButtonDown(eg::Button::Enter)));

	if (hovered)
		m_highlightIntensity = std::min(m_highlightIntensity + frameArgs.dt / style::HoverAnimationTime, 1.0f);
	else
		m_highlightIntensity = std::max(m_highlightIntensity - frameArgs.dt / style::HoverAnimationTime, 0.0f);

	if (hovered && !m_wasHovered)
		PlayButtonHoverSound();
	m_wasHovered = hovered;

	if (clicked && onClick)
		onClick();
}

void Button::Draw(const GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch) const
{
	float inflate = m_highlightIntensity * style::ButtonInflatePercent;
	float fontScale = 0.75f * (1 + inflate);

	eg::Rectangle rect(position, width, height);

	DrawBackground(spriteBatch, rect, m_highlightIntensity, enabled);

	glm::vec2 extents = glm::vec2(style::UIFont->GetTextExtents(text).x, style::UIFont->Size()) * fontScale;
	eg::ColorLin textColor(eg::Color::White);
	if (!enabled)
		textColor.a *= 0.5f;

	glm::vec2 textPos = rect.Center() - extents / 2.0f + glm::vec2(0, 3);
	spriteBatch.DrawText(
		*style::UIFont, text, textPos, textColor, fontScale, nullptr,
		eg::TextFlags::NoPixelAlign | eg::TextFlags::DropShadow);
}

void Button::DrawBackground(
	eg::SpriteBatch& spriteBatch, const eg::Rectangle& rect, float highlightIntensity, bool enabled)
{
	float inflate = highlightIntensity * style::ButtonInflatePercent;

	glm::vec2 inflateSize = inflate * rect.Size();
	eg::Rectangle inflatedRect(
		rect.x - inflateSize.x, rect.y - inflateSize.y, rect.w + inflateSize.x * 2, rect.h + inflateSize.y * 2);

	eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, highlightIntensity);
	if (!enabled)
		backColor.a *= 0.5f;

	spriteBatch.DrawRect(inflatedRect, backColor);
}
