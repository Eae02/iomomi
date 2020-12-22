#include "ComboBox.hpp"
#include "../GuiCommon.hpp"

ComboBox* ComboBox::current;

static constexpr int DROP_DOWN_HEIGHT = 25;
static constexpr int OPTION_HEIGHT = 25;

void ComboBox::Update(float dt, bool allowInteraction)
{
	m_lastFrameUpdated = eg::FrameIdx();
	
	UpdateBase(allowInteraction);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, m_hovered || current == this);
	
	bool clicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft) && allowInteraction;
	
	if (current == this)
	{
		eg::Rectangle optionRect = m_rectangle;
		optionRect.h = DROP_DOWN_HEIGHT;
		for (size_t i = 0; i < options.size(); i++)
		{
			optionRect.y -= DROP_DOWN_HEIGHT;
			bool optHovered = allowInteraction && optionRect.Contains(glm::vec2(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY()));
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
		else if (m_hovered)
		{
			current = this;
			m_optionHighlightIntensity.assign(options.size(), 0.0f);
		}
	}
	
	AnimateProperty(m_expandProgress, dt, 0.1f, current == this);
}

void ComboBox::Draw(eg::SpriteBatch& spriteBatch) const
{
	DrawBase(spriteBatch, m_highlightIntensity, options[getValue()]);
	
	if (!warning.empty())
	{
		glm::vec2 textExtents = style::UIFont->GetTextExtents(warning) * FONT_SCALE;
		glm::vec2 textPos(m_rectangle.MaxX() - textExtents.x - 5, m_rectangle.CenterY() - textHeight / 2.0f);
		
		eg::ColorLin textColor(eg::ColorSRGB::FromHex(0xe8b29a));
		spriteBatch.DrawText(*style::UIFont, warning, textPos, textColor, FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	}
}

void ComboBox::DrawOverlay(eg::SpriteBatch& spriteBatch) const
{
	if (m_lastFrameUpdated != eg::FrameIdx())
	{
		ComboBox::current = nullptr;
		return;
	}
	
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
