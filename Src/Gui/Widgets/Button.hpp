#pragma once

#include <functional>

class Button
{
public:
	Button() = default;
	Button(std::string _text, std::function<void()> _onClick)
		: text(std::move(_text)), onClick(std::move(_onClick)) { }
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	std::string text;
	
	glm::vec2 position;
	float width;
	
	static constexpr float height = 40;
	
	bool hasKeyboardFocus = false;
	
	std::function<void()> onClick;
	
private:
	float m_highlightIntensity = 0;
};
