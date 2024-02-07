#pragma once

class SubtitleWidget
{
public:
	explicit SubtitleWidget(std::string text) : m_text(std::move(text)) {}

	void Update(const struct GuiFrameArgs& frameArgs, bool allowInteraction) {}
	void Draw(const struct GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch) const;

	glm::vec2 position;
	float width;

	static constexpr float height = 40;

private:
	std::string m_text;
};
