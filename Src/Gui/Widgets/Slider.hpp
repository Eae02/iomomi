#pragma once

#include <functional>
#include "OptionWidgetBase.hpp"

class Slider : public OptionWidgetBase
{
public:
	Slider();
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	std::function<float()> getValue;
	std::function<void(float)> setValue;
	
	float min = 0;
	float max = 1;
	float increment = 0;
	
	bool displayValue = false;
	const char* valueSuffix = nullptr;
	
private:
	
};
