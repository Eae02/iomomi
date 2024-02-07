#pragma once

#include <functional>

#include "../Scroll.hpp"
#include "OptionWidgetBase.hpp"

class ComboBox : public OptionWidgetBase
{
public:
	ComboBox();

	void Update(const GuiFrameArgs& frameArgs, bool allowInteraction);
	void Draw(const GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch) const;

	void DrawOverlay(const GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch) const;

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
