#include "ComboBox.hpp"
#include "../GuiCommon.hpp"

ComboBox* ComboBox::current;

static constexpr int DROP_DOWN_HEIGHT = 25;
static constexpr int OPTION_HEIGHT = 25;
static constexpr float FONT_SCALE = 0.6f;

static float textHeight = 0;

void ComboBox::Update(float dt, bool allowInteraction)
{
	if (textHeight == 0)
	{
		textHeight = (float)style::UIFont->Size() * FONT_SCALE;
	}
	
	m_rectangle = eg::Rectangle(position.x + width / 2, position.y, width / 2, height);
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	bool hovered = allowInteraction && m_rectangle.Contains(flippedCursorPos);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, hovered || current == this);
	
	bool clicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft) && allowInteraction;
	
	if (current == this)
	{
		eg::Rectangle optionRect = m_rectangle;
		optionRect.h = DROP_DOWN_HEIGHT;
		for (size_t i = 0; i < options.size(); i++)
		{
			optionRect.y -= DROP_DOWN_HEIGHT;
			bool optHovered = allowInteraction && optionRect.Contains(flippedCursorPos);
			AnimateProperty(m_optionHighlightIntensity[i], dt, style::HoverAnimationTime, optHovered);
			if (optHovered && clicked)
				setValue(i);
		}
	}
	
	if (clicked)
	{
		if (current == this)
		{
			current = nullptr;
		}
		else if (hovered)
		{
			current = this;
			m_optionHighlightIntensity.assign(options.size(), 0.0f);
		}
	}
	
	AnimateProperty(m_expandProgress, dt, 0.1f, current == this);
}

void ComboBox::Draw(eg::SpriteBatch& spriteBatch) const
{
	//Draws the background rectangle
	eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, m_highlightIntensity);
	spriteBatch.DrawRect(m_rectangle, backColor);
	
	//Draws the label text
	float labelTextWidth = style::UIFont->GetTextExtents(label).x * FONT_SCALE;
	glm::vec2 labelTextPos(m_rectangle.x - labelTextWidth - 15, m_rectangle.CenterY() - textHeight / 2.0f);
	spriteBatch.DrawText(*style::UIFont, label, labelTextPos, eg::ColorLin(eg::Color::White.ScaleAlpha(0.7f)),
		FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	
	//Draws the current option text
	const std::string& optionText = options[getValue()];
	glm::vec2 textPos(m_rectangle.x + (m_rectangle.h - textHeight) / 2.0f, m_rectangle.CenterY() - textHeight / 2.0f);
	spriteBatch.DrawText(*style::UIFont, optionText, textPos, eg::ColorLin(eg::Color::White),
		FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
}

void ComboBox::DrawOverlay(eg::SpriteBatch& spriteBatch) const
{
	float expandProgress = glm::smoothstep(0.0f, 1.0f, m_expandProgress);
	int dropDownHeight = (int)std::round(std::min(DROP_DOWN_HEIGHT, (int)options.size()) * OPTION_HEIGHT * expandProgress);
	spriteBatch.PushScissor((int)m_rectangle.x, (int)m_rectangle.y - dropDownHeight, (int)m_rectangle.w, dropDownHeight);
	
	eg::Rectangle optionRect = m_rectangle;
	optionRect.h = DROP_DOWN_HEIGHT;
	
	glm::vec2 textPos(optionRect.x + 7, optionRect.CenterY() - textHeight / 2.0f + 2);
	
	int currentIndex = getValue();
	for (size_t i = 0; i < options.size(); i++)
	{
		if (i != 0)
		{
			spriteBatch.DrawLine(optionRect.Min(), optionRect.MaxXMinY(), eg::ColorLin(1, 1, 1, 0.05f));
		}
		
		optionRect.y -= DROP_DOWN_HEIGHT;
		textPos.y -= DROP_DOWN_HEIGHT;
		
		float highlightIntensity = m_optionHighlightIntensity[i];
		if ((int)i == currentIndex)
			highlightIntensity = glm::mix(0.3f, 1.0f, highlightIntensity);
		eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, highlightIntensity);
		backColor.a = 1;
		
		spriteBatch.DrawRect(optionRect, backColor);
		
		float textA = glm::mix(0.75f, 1.0f, highlightIntensity);
		
		glm::vec2 realTextPos = textPos;
		realTextPos.x += glm::smoothstep(0.0f, 1.0f, m_optionHighlightIntensity[i]) * 3.0f;
		spriteBatch.DrawText(*style::UIFont, options[i], realTextPos, eg::ColorLin(1, 1, 1, textA), FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	}
	
	spriteBatch.PopScissor();
}
