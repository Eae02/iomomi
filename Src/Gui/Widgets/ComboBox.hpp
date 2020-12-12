#pragma once

#include <functional>
#include "OptionWidgetBase.hpp"

class ComboBox : public OptionWidgetBase
{
public:
	ComboBox() = default;
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	void DrawOverlay(eg::SpriteBatch& spriteBatch) const;
	
	std::vector<std::string> options;
	
	std::function<int()> getValue;
	std::function<void(int)> setValue;
	
	static ComboBox* current;
	
private:
	float m_highlightIntensity = 0;
	float m_expandProgress = 0;
	std::vector<float> m_optionHighlightIntensity;
};
