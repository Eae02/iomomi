#include "Scroll.hpp"

#include "GuiCommon.hpp"

static constexpr int SCROLL_BAR_W = 6;

static constexpr float SCROLL_SPEED = 1500;

static constexpr float BRIGHTNESS_ANIM_SPEED = 0.1f;

void ScrollPanel::SetScroll(float s)
{
	scroll = glm::clamp(s, 0.0f, static_cast<float>(m_maxScroll));

	m_barAreaRectangle = eg::Rectangle(
		screenRectangle.MaxX() - SCROLL_BAR_W - marginX, screenRectangle.y + marginY, SCROLL_BAR_W,
		screenRectangle.h - marginY * 2);

	const float barHeight = m_barAreaRectangle.h * screenRectangle.h / contentHeight;
	const float barYOffset = (scroll / static_cast<float>(m_maxScroll)) * (m_barAreaRectangle.h - barHeight);
	m_barActiveRectangle = eg::Rectangle(
		m_barAreaRectangle.x + 1, m_barAreaRectangle.y + m_barAreaRectangle.h - barYOffset - barHeight,
		SCROLL_BAR_W - 2, barHeight);
}

void ScrollPanel::Update(float dt, bool canInteract)
{
	m_maxScroll = std::max(contentHeight - screenRectangle.h, 0.0f);

	velocity *= 1 - std::min(dt * 10.0f, 1.0f);
	if (canInteract && !isDragging)
	{
		velocity += (eg::InputState::Previous().scrollY - eg::InputState::Current().scrollY) * SCROLL_SPEED * 0.5f;
	}
	velocity = glm::clamp(velocity, -SCROLL_SPEED, SCROLL_SPEED);
	SetScroll(scroll + velocity * dt);

	if (m_maxScroll > 0 && canInteract)
	{
		glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());

		eg::Rectangle dragGrabRectangle(
			m_barActiveRectangle.x - dragGrabMarginX, m_barActiveRectangle.y,
			m_barActiveRectangle.w + dragGrabMarginX * 2, m_barActiveRectangle.h);
		const bool canBeginDrag = !isDragging && dragGrabRectangle.Contains(flippedCursorPos);

		if (canBeginDrag && eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
		{
			isDragging = true;
		}
		else if (isDragging)
		{
			if (!eg::IsButtonDown(eg::Button::MouseLeft))
			{
				isDragging = false;
			}
			else
			{
				velocity = 0;
				SetScroll(
					scroll + (float)eg::CursorDeltaY() * m_maxScroll / (m_barAreaRectangle.h - m_barActiveRectangle.h));
			}
		}

		float barBrightness = 1.0f;
		if (isDragging)
			barBrightness = 1.4f;
		else if (canBeginDrag)
			barBrightness = 1.2f;
		m_barBrightness = eg::AnimateTo(m_barBrightness, barBrightness, dt / BRIGHTNESS_ANIM_SPEED);
	}
	else
	{
		isDragging = false;
		m_barBrightness = 1;
	}
}

void ScrollPanel::Draw(eg::SpriteBatch& spriteBatch) const
{
	if (m_maxScroll > 0)
	{
		spriteBatch.DrawRect(m_barAreaRectangle, style::ButtonColorDefault);
		spriteBatch.DrawRect(m_barActiveRectangle, style::ButtonColorHover.ScaleRGB(m_barBrightness));
	}
}
