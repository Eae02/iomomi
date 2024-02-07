#pragma once

#include <functional>

#include "../GuiCommon.hpp"

class Button
{
public:
	Button() = default;
	Button(std::string _text, std::function<void()> _onClick) : text(std::move(_text)), onClick(std::move(_onClick)) {}

	void Update(const GuiFrameArgs& frameArgs, bool allowInteraction);
	void Draw(const GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch) const;

	static void DrawBackground(
		eg::SpriteBatch& spriteBatch, const eg::Rectangle& rectangle, float highlightIntensity, bool enabled = true);

	std::string text;

	glm::vec2 position;
	float width = 0;

	bool enabled = true;

	static constexpr float height = 40;

	bool hasKeyboardFocus = false;

	std::function<void()> onClick;

private:
	float m_highlightIntensity = 0;
	uint64_t m_lastUpdateFrameIdx = 0;
	bool m_wasHovered = false;
};
