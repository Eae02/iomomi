#pragma once

#include <functional>

class ComboBox
{
public:
	ComboBox() = default;
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	static void DrawButton(eg::SpriteBatch& spriteBatch, const eg::Rectangle& rectangle, float highlightIntensity,
                           std::string_view label, std::string_view optionText);
	
	void DrawOverlay(eg::SpriteBatch& spriteBatch) const;
	
	std::vector<std::string> options;
	
	std::function<int()> getValue;
	std::function<void(int)> setValue;
	
	static ComboBox* current;
	
	std::string label;
	std::string warning;
	glm::vec2 position;
	float width;
	
	static constexpr float height = 30;
	
private:
	eg::Rectangle m_rectangle;
	float m_highlightIntensity = 0;
	float m_expandProgress = 0;
	std::vector<float> m_optionHighlightIntensity;
};
