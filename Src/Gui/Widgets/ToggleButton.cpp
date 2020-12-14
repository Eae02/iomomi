#include "ToggleButton.hpp"
#include "../GuiCommon.hpp"

static constexpr float TEXT_SWAP_ANIM_TIME = 0.15f;
static constexpr float TEXT_SWAP_MOVE_DIST = 5;

void ToggleButton::Update(float dt, bool allowInteraction)
{
	UpdateBase(allowInteraction);
	
	bool currentValue = getValue();
	const float targetTextSwapProgress = currentValue ? 1 : -1;
	
	AnimateProperty(m_highlightIntensity, dt, style::HoverAnimationTime, m_hovered);
	if (m_hovered && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		currentValue = !currentValue;
		setValue(currentValue);
	}
	
	if (eg::FrameIdx() > m_prevFrameUpdated + 1)
	{
		m_textSwapProgress = targetTextSwapProgress;
	}
	else
	{
		m_textSwapProgress = eg::AnimateTo(m_textSwapProgress, targetTextSwapProgress, 2 * dt / TEXT_SWAP_ANIM_TIME);
	}
	m_prevFrameUpdated = eg::FrameIdx();
}

void ToggleButton::Draw(eg::SpriteBatch& spriteBatch) const
{
	DrawBase(spriteBatch, m_highlightIntensity, "");//getValue() ? trueString : falseString);
	
	// -1        = 0
	// -0.0001   = -md
	//  0.0001   = md
	// 1         = 0
	
	glm::vec2 textPos = GetTextPos();
	textPos.x += (1 - std::abs(m_textSwapProgress)) * TEXT_SWAP_MOVE_DIST * glm::sign(m_textSwapProgress);
	eg::ColorLin textColor(1, 1, 1, std::abs(m_textSwapProgress));
	const std::string* text = m_textSwapProgress > 0 ? &trueString : &falseString;
	spriteBatch.DrawText(*style::UIFont, *text, textPos, textColor, FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
}
