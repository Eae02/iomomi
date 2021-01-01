#pragma once

class OptionWidgetBase
{
public:
	OptionWidgetBase() = default;
	
	std::string label;
	std::string warning;
	glm::vec2 position;
	float width;
	float height = 30;
	
	static constexpr float FONT_SCALE = 0.6f;
	
protected:
	void UpdateBase(bool allowInteraction);
	
	void DrawBase(eg::SpriteBatch& spriteBatch, float highlightIntensity, std::string_view valueText) const;
	
	static glm::vec2 GetTextPos(const eg::Rectangle& rectangle);
	glm::vec2 GetTextPos() const { return GetTextPos(m_rectangle); }
	
	eg::Rectangle m_rectangle;
	bool m_hovered = false;
	
	static float textHeight;
};