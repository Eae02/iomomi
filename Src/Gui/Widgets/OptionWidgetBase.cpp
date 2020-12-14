#include "OptionWidgetBase.hpp"
#include "../GuiCommon.hpp"

float OptionWidgetBase::textHeight = 0;

void OptionWidgetBase::UpdateBase(bool allowInteraction)
{
	if (textHeight == 0)
	{
		textHeight = (float)style::UIFont->Size() * FONT_SCALE;
	}
	
	m_rectangle = eg::Rectangle(position.x + width / 2, position.y, width / 2, height);
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	m_hovered = allowInteraction && m_rectangle.Contains(flippedCursorPos);
}

void OptionWidgetBase::DrawBase(eg::SpriteBatch& spriteBatch, float highlightIntensity, std::string_view valueText) const
{
	//Draws the background rectangle
	eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, highlightIntensity);
	spriteBatch.DrawRect(m_rectangle, backColor);
	
	//Draws the label text
	float labelTextWidth = style::UIFont->GetTextExtents(label).x * FONT_SCALE;
	glm::vec2 labelTextPos(m_rectangle.x - labelTextWidth - 15, m_rectangle.CenterY() - textHeight / 2.0f);
	spriteBatch.DrawText(*style::UIFont, label, labelTextPos, eg::ColorLin(eg::Color::White.ScaleAlpha(0.8f)),
		FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	
	//Draws the current value text
	if (!valueText.empty())
	{
		spriteBatch.DrawText(*style::UIFont, valueText, GetTextPos(), eg::ColorLin(eg::Color::White),
		                     FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	}
}

glm::vec2 OptionWidgetBase::GetTextPos() const
{
	return glm::vec2(m_rectangle.x + (m_rectangle.h - textHeight) / 2.0f, m_rectangle.CenterY() - textHeight / 2.0f + 1);
}
