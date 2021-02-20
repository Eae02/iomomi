#include "Slider.hpp"
#include "../GuiCommon.hpp"

static Slider* currentSlider = nullptr;

Slider::Slider()
{
	height = 25;
}

void Slider::Update(float dt, bool allowInteraction)
{
	UpdateBase(allowInteraction, false);
	
	constexpr float RECTANGLE_H_PERCENT = 0.8f;
	m_rectangle.y = m_rectangle.CenterY() - m_rectangle.h * RECTANGLE_H_PERCENT * 0.5f;
	m_rectangle.h *= RECTANGLE_H_PERCENT;
	
	if (eg::IsButtonDown(eg::Button::MouseLeft))
	{
		if (m_hovered && !eg::WasButtonDown(eg::Button::MouseLeft))
		{
			currentSlider = this;
		}
		if (currentSlider == this)
		{
			float value01 = glm::clamp((eg::CursorX() - m_rectangle.x) / m_rectangle.w, 0.0f, 1.0f);
			float value = glm::mix(min, max, value01);
			if (increment != 0)
			{
				value = glm::clamp(std::round(value / increment) * increment, min, max);
			}
			setValue(value);
		}
	}
	else
	{
		currentSlider = nullptr;
	}
}

void Slider::Draw(eg::SpriteBatch& spriteBatch) const
{
	DrawBase(spriteBatch, 0, std::string_view());
	
	float value = getValue();
	
	eg::Rectangle activeRect(
		m_rectangle.x,
		m_rectangle.y,
		m_rectangle.w * glm::clamp((value - min) / (max - min), 0.0f, 1.0f),
		m_rectangle.h
	);
	spriteBatch.DrawRect(activeRect, eg::ColorLin(1, 1, 1, 0.8f));
	
	if (getDisplayValueString)
	{
		std::string_view valueStr = getDisplayValueString(value);
		
		constexpr float VALUE_BORDER_SPACING = 5;
		float textWidth = style::UIFontSmall->GetTextExtents(valueStr).x;
		
		glm::vec2 textPos;
		textPos.y = activeRect.y + 5;
		eg::ColorLin textColor;
		if (activeRect.w + textWidth + VALUE_BORDER_SPACING < m_rectangle.w)
		{
			textPos.x = activeRect.MaxX() + VALUE_BORDER_SPACING;
			textColor = eg::ColorLin(1, 1, 1, 1);
		}
		else
		{
			textPos.x = activeRect.MaxX() - textWidth - VALUE_BORDER_SPACING;
			textColor = eg::ColorLin(0, 0, 0, 1);
		}
		spriteBatch.DrawText(*style::UIFontSmall, valueStr, textPos, textColor);
	}
}
