#pragma once

#include <functional>

class ToggleButton
{
public:
	ToggleButton() = default;
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	std::function<bool()> getValue;
	std::function<void(bool)> setValue;
	
	std::string trueString;
	std::string falseString;
	
	std::string label;
	glm::vec2 position;
	float width;
	
	static constexpr float height = 30;
	
private:
	eg::Rectangle m_rectangle;
	float m_highlightIntensity = 0;
};
