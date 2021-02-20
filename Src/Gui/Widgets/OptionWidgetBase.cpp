#include "OptionWidgetBase.hpp"
#include "../GuiCommon.hpp"
#include "../../AudioPlayers.hpp"

float OptionWidgetBase::textHeight = 0;

static constexpr float LABEL_WIDTH_PERCENT = 0.4f;

void OptionWidgetBase::UpdateBase(bool allowInteraction, bool playHoverSound)
{
	if (textHeight == 0)
	{
		textHeight = (float)style::UIFont->Size() * FONT_SCALE;
	}
	
	m_rectangle = eg::Rectangle(position.x + width * LABEL_WIDTH_PERCENT, position.y, width * (1 - LABEL_WIDTH_PERCENT), height);
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	bool nowHovered = allowInteraction && m_rectangle.Contains(flippedCursorPos);
	if (nowHovered && !m_hovered && playHoverSound)
		PlayButtonHoverSound();
	m_hovered = nowHovered;
}

void OptionWidgetBase::DrawBase(eg::SpriteBatch& spriteBatch, float highlightIntensity, std::string_view valueText, float maxValueWidth) const
{
	//Draws the background rectangle
	eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, highlightIntensity);
	spriteBatch.DrawRect(m_rectangle, backColor);
	
	const eg::ColorLin labelMainColor(eg::Color::White.ScaleAlpha(0.8f));
	const eg::ColorLin labelFadedColor(eg::Color::White.ScaleAlpha(0.5f));
	
	//Draws the label text
	float labelTextWidth = style::UIFont->GetTextExtents(label).x * FONT_SCALE;
	glm::vec2 labelTextPos(m_rectangle.x - labelTextWidth - 15, m_rectangle.CenterY() - textHeight / 2.0f);
	spriteBatch.DrawText(*style::UIFont, label, labelTextPos, labelMainColor,
		FONT_SCALE, nullptr, eg::TextFlags::DropShadow, &labelFadedColor);
	
	//Draws the current value text
	if (!valueText.empty() && maxValueWidth >= 0)
	{
		glm::vec2 textPos = GetTextPos();
		bool applyScissor = maxValueWidth != 0 && maxValueWidth < style::UIFont->GetTextExtents(valueText).x * FONT_SCALE;
		if (applyScissor)
		{
			spriteBatch.PushScissor(textPos.x, m_rectangle.y, maxValueWidth, height);
		}
		
		const eg::ColorLin valueFadedColor(eg::Color::White.ScaleAlpha(0.5f));
		spriteBatch.DrawText(*style::UIFont, valueText, GetTextPos(), eg::ColorLin(eg::Color::White),
		                     FONT_SCALE, nullptr, eg::TextFlags::DropShadow, &valueFadedColor);
		
		if (applyScissor)
		{
			spriteBatch.PopScissor();
		}
	}
}

glm::vec2 OptionWidgetBase::GetTextPos(const eg::Rectangle& rectangle)
{
	return glm::vec2(rectangle.x + (rectangle.h - textHeight) / 2.0f, rectangle.CenterY() - textHeight / 2.0f + 1);
}
