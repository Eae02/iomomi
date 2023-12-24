#pragma once

class SubtitleWidget
{
public:
	explicit SubtitleWidget(std::string text) : m_text(std::move(text)) {}

	void Update(float dt, bool allowInteraction) {}
	void Draw(eg::SpriteBatch& spriteBatch) const;

	glm::vec2 position;
	float width;

	static constexpr float height = 40;

private:
	std::string m_text;
};
