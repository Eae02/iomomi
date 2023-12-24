#pragma once

namespace style
{
extern const eg::ColorLin ButtonColorDefault;
extern const eg::ColorLin ButtonColorHover;

constexpr float ButtonInflatePercent = 0.05f;
constexpr float HoverAnimationTime = 0.06f;

extern const eg::SpriteFont* UIFont;
extern const eg::SpriteFont* UIFontSmall;
} // namespace style

inline void AnimateProperty(float& value, float dt, float animTime, bool active)
{
	value = glm::clamp(value + (active ? dt : -dt) / animTime, 0.0f, 1.0f);
}

constexpr float BUTTON_HOVER_VOLUME = 0.4f;

void PlayButtonHoverSound();
