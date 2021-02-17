#include "ComboBox.hpp"
#include "../GuiCommon.hpp"

ComboBox* ComboBox::current;

static constexpr int DROP_DOWN_HEIGHT = 25;
static constexpr int OPTION_HEIGHT = 25;

static const eg::Texture* arrowTexture;

void ComboBox::Update(float dt, bool allowInteraction)
{
	if (!m_initialValue.has_value() && restartRequiredIfChanged)
		m_initialValue = getValue();
	
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
	if (arrowTexture == nullptr)
	{
		arrowTexture = &eg::GetAsset<eg::Texture>("Textures/UI/ComboArrow.png");
	}
	
	int value = getValue();
	
	std::string_view warningToShow = warning;
	if (warningToShow.empty() && m_initialValue.has_value() && restartRequiredIfChanged && value != *m_initialValue)
	{
		warningToShow = "Restart req.";
	}
	glm::vec2 warningTextExtents = style::UIFont->GetTextExtents(warningToShow) * FONT_SCALE;
	
	float maxValueWidth = m_rectangle.w - height - 10;
	if (!warningToShow.empty())
		maxValueWidth -= warningTextExtents.x;
	
	DrawBase(spriteBatch, m_highlightIntensity, options[value], maxValueWidth);
	
	spriteBatch.Draw(*arrowTexture, glm::vec2(m_rectangle.MaxX() - height / 2.0f, m_rectangle.CenterY()), eg::ColorLin(1, 1, 1, 0.8f),
	                 1, eg::SpriteFlags::None, m_expandProgress * eg::PI, glm::vec2(height / 2.0f));
	
	if (!warningToShow.empty())
	{
		glm::vec2 textPos(m_rectangle.MaxX() - warningTextExtents.x - height, m_rectangle.CenterY() - textHeight / 2.0f);
		
		eg::ColorLin textColor(eg::ColorSRGB::FromHex(0xe8b29a));
		spriteBatch.DrawText(*style::UIFont, warningToShow, textPos, textColor, FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
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
	const int dropDownHeight = (int)std::round(std::min(DROP_DOWN_HEIGHT, (int)options.size()) * OPTION_HEIGHT * expandProgress);
	const int dropDownMinY = (int)m_rectangle.y - dropDownHeight;
	
	spriteBatch.PushScissor((int)m_rectangle.x, dropDownMinY, (int)m_rectangle.w, dropDownHeight);
	
	const eg::ColorLin DIVIDER_COLOR = eg::ColorLin::Mix(style::ButtonColorDefault, eg::ColorLin(eg::Color::White), 0.25f);
	
	eg::Rectangle optionRect = m_rectangle;
	optionRect.h = DROP_DOWN_HEIGHT;
	
	glm::vec2 textPos(optionRect.x + 7, optionRect.CenterY() - textHeight / 2.0f + 2);
	
	int currentIndex = getValue();
	for (size_t i = 0; i < options.size(); i++)
	{
		if (i != 0)
		{
			spriteBatch.DrawLine(optionRect.Min(), optionRect.MaxXMinY(), DIVIDER_COLOR);
		}
		
		optionRect.y -= DROP_DOWN_HEIGHT;
		textPos.y -= DROP_DOWN_HEIGHT;
		
		float highlightIntensity = m_optionHighlightIntensity[i];
		if ((int)i == currentIndex)
			highlightIntensity = glm::mix(0.3f, 1.0f, highlightIntensity);
		eg::ColorLin backColor = eg::ColorLin::Mix(style::ButtonColorDefault, style::ButtonColorHover, highlightIntensity);
		backColor.a = 1;
		
		spriteBatch.DrawRect(optionRect, backColor);
		
		const eg::ColorLin mainColor(1, 1, 1, glm::mix(0.75f, 1.0f, highlightIntensity));
		const eg::ColorLin secondColor(1, 1, 1, mainColor.a * 0.5f);
		
		glm::vec2 realTextPos = textPos;
		realTextPos.x += glm::smoothstep(0.0f, 1.0f, m_optionHighlightIntensity[i]) * 3.0f;
		spriteBatch.DrawText(*style::UIFont, options[i], realTextPos, mainColor, FONT_SCALE, nullptr, eg::TextFlags::DropShadow, &secondColor);
	}
	spriteBatch.DrawRectBorder(eg::Rectangle((int)m_rectangle.x, dropDownMinY, (int)m_rectangle.w, dropDownHeight + 5), DIVIDER_COLOR);
	
	spriteBatch.PopScissor();
}
