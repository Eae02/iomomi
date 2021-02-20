#include "Scroll.hpp"
#include "GuiCommon.hpp"

void ScrollPanel::Update(float dt, bool canInteract)
{
	const float maxScroll = std::max(contentHeight - screenRectangle.h, 0.0f);
	constexpr float SCROLL_SPEED = 1500;
	velocity *= 1 - std::min(dt * 10.0f, 1.0f);
	if (canInteract)
		velocity += (eg::InputState::Previous().scrollY - eg::InputState::Current().scrollY) * SCROLL_SPEED * 0.5f;
	velocity = glm::clamp(velocity, -SCROLL_SPEED, SCROLL_SPEED);
	scroll = glm::clamp(scroll + velocity * dt, 0.0f, (float)maxScroll);
}

void ScrollPanel::Draw(eg::SpriteBatch& spriteBatch) const
{
	const float maxScroll = std::max(contentHeight - screenRectangle.h, 0.0f);
	if (maxScroll > 0)
	{
		constexpr int SCROLL_BAR_W = 6;
		const eg::Rectangle scrollAreaRect(
			screenRectangle.MaxX() - SCROLL_BAR_W - marginX,
			screenRectangle.y + marginY,
			SCROLL_BAR_W,
			screenRectangle.h - marginY * 2
		);
		spriteBatch.DrawRect(scrollAreaRect, style::ButtonColorDefault);
		
		const float barHeight = scrollAreaRect.h * screenRectangle.h / contentHeight;
		const float barYOffset = (scroll / (float)maxScroll) * (scrollAreaRect.h - barHeight);
		const eg::Rectangle scrollBarRect(
			scrollAreaRect.x + 1,
			scrollAreaRect.y + scrollAreaRect.h - barYOffset - barHeight,
			SCROLL_BAR_W - 2,
			barHeight
		);
		spriteBatch.DrawRect(scrollBarRect, style::ButtonColorHover);
	}
}
