#pragma once

#include <functional>
#include "OptionWidgetBase.hpp"
#include "../Scroll.hpp"

class ComboBox : public OptionWidgetBase
{
public:
	ComboBox();
	
	void Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	void DrawOverlay(eg::SpriteBatch& spriteBatch) const;
	
	std::vector<std::string> options;
	
	std::function<int()> getValue;
	std::function<void(int)> setValue;
	
	bool restartRequiredIfChanged = false;
	
	static ComboBox* current;
	
private:
	float m_highlightIntensity = 0;
	float m_expandProgress = 0;
	int m_prevOptionHovered = -1;
	std::vector<float> m_optionHighlightIntensity;
	uint64_t m_lastFrameUpdated = 0;
	std::optional<int> m_initialValue;
	ScrollPanel m_scrollPanel;
};
