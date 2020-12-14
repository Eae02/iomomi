#pragma once

#include <functional>
#include "OptionWidgetBase.hpp"

class ToggleButton : public OptionWidgetBase
{
public:
	ToggleButton() = default;
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	std::function<bool()> getValue;
	std::function<void(bool)> setValue;
	
	std::string trueString;
	std::string falseString;
	
private:
	float m_highlightIntensity = 0;
	float m_textSwapProgress = 0;
	uint64_t m_prevFrameUpdated = 0;
};
