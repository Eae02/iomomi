#include "ComboBox.hpp"
#include "../GuiCommon.hpp"

ComboBox* ComboBox::current;

static constexpr int OPTION_HEIGHT = 25;
static constexpr float MAX_DROP_DOWN_HEIGHT = 300;

static const eg::Texture* arrowTexture;

ComboBox::ComboBox()
{
	m_scrollPanel.marginX = 3;
	m_scrollPanel.marginY = 3;
}

void ComboBox::Update(float dt, bool allowInteraction)
{
	if (!m_initialValue.has_value() && restartRequiredIfChanged)
		m_initialValue = getValue();
	
	m_lastFrameUpdated = eg::FrameIdx();
	
	const glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	UpdateBase(allowInteraction);
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, m_hovered || current == this);
	
	AnimateProperty(m_expandProgress, dt, 0.1f, current == this);
	const float expandProgressSmooth = glm::smoothstep(0.0f, 1.0f, m_expandProgress);
	
	m_scrollPanel.contentHeight = options.size() * OPTION_HEIGHT;
	m_scrollPanel.screenRectangle.x = m_rectangle.x;
	m_scrollPanel.screenRectangle.w = m_rectangle.w;
	m_scrollPanel.screenRectangle.h = std::min(std::round(options.size() * OPTION_HEIGHT * expandProgressSmooth), MAX_DROP_DOWN_HEIGHT);
	m_scrollPanel.screenRectangle.y = m_rectangle.y - m_scrollPanel.screenRectangle.h;
	constexpr float SCREEN_RECT_MIN_Y = 20;
	if (m_scrollPanel.screenRectangle.y < SCREEN_RECT_MIN_Y)
	{
		m_scrollPanel.screenRectangle.h -= m_scrollPanel.screenRectangle.y - SCREEN_RECT_MIN_Y;
		m_scrollPanel.screenRectangle.y = SCREEN_RECT_MIN_Y;
	}
	
	m_scrollPanel.Update(dt);
	
	bool clicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft) && allowInteraction;
	
	if (m_optionHighlightIntensity.size() < options.size())
	{
		m_optionHighlightIntensity.resize(options.size(), 0.0f);
	}
	
	bool canInteractOptions = (allowInteraction && current == this && m_scrollPanel.screenRectangle.Contains(flippedCursorPos));
	int hoveredOptionIndex = -1;
	eg::Rectangle optionRect = m_rectangle;
	optionRect.h = OPTION_HEIGHT - 1;
	optionRect.y += m_scrollPanel.scroll;
	for (size_t i = 0; i < options.size(); i++)
	{
		optionRect.y -= OPTION_HEIGHT;
		bool optHovered = canInteractOptions && optionRect.Contains(flippedCursorPos);
		AnimateProperty(m_optionHighlightIntensity[i], dt, style::HoverAnimationTime, optHovered);
		if (optHovered && hoveredOptionIndex == -1)
			hoveredOptionIndex = i;
	}
	if (hoveredOptionIndex != -1)
	{
		if (m_prevOptionHovered != hoveredOptionIndex)
			PlayButtonHoverSound();
		if (clicked)
			setValue(hoveredOptionIndex);
	}
	m_prevOptionHovered = hoveredOptionIndex;
	
	if (clicked)
	{
		if (current == this)
		{
			current = nullptr;
		}
		else if (m_hovered)
		{
			current = this;
		}
	}
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
#ifdef __EMSCRIPTEN__
		warningToShow = "Reload req.";
#else
		warningToShow = "Restart req.";
#endif
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
	
	spriteBatch.PushScissor(
		(int)m_scrollPanel.screenRectangle.x,
		(int)m_scrollPanel.screenRectangle.y,
		(int)m_scrollPanel.screenRectangle.w,
		(int)m_scrollPanel.screenRectangle.h
	);
	
	const eg::ColorLin DIVIDER_COLOR = eg::ColorLin::Mix(style::ButtonColorDefault, eg::ColorLin(eg::Color::White), 0.25f);
	
	eg::Rectangle optionRect = m_rectangle;
	optionRect.h = OPTION_HEIGHT;
	optionRect.y += m_scrollPanel.scroll;
	
	glm::vec2 textPos(optionRect.x + 7, optionRect.CenterY() - textHeight / 2.0f + 2);
	
	int currentIndex = getValue();
	for (size_t i = 0; i < options.size(); i++)
	{
		if (i != 0)
		{
			spriteBatch.DrawLine(optionRect.Min(), optionRect.MaxXMinY(), DIVIDER_COLOR);
		}
		
		optionRect.y -= OPTION_HEIGHT;
		textPos.y -= OPTION_HEIGHT;
		
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
	
	if (m_expandProgress == 1)
		m_scrollPanel.Draw(spriteBatch);
	
	eg::Rectangle borderRect = m_scrollPanel.screenRectangle;
	borderRect.h += 5;
	spriteBatch.DrawRectBorder(borderRect, DIVIDER_COLOR);
	
	spriteBatch.PopScissor();
}
