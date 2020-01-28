#pragma once

#include "Button.hpp"
#include "ComboBox.hpp"
#include "SubtitleWidget.hpp"

using Widget = std::variant<Button, ComboBox, SubtitleWidget>;

class WidgetList
{
public:
	explicit WidgetList(float width)
		: m_width(width) { }
	
	float Width() const { return m_width; }
	float Height() const { return m_height; }
	
	void AddSpacing(float spacing)
	{
		m_nextY += spacing;
	}
	
	void AddWidget(Widget widget);
	
	template <typename CallbackTp>
	void AddWidgetFn(CallbackTp fn)
	{
		AddWidget(fn());
	}
	
	bool Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	glm::vec2 position;
	glm::vec2 relativeOffset;
	
	float maxHeight = INFINITY;
	
private:
	float m_width;
	float m_height = 0;
	float m_nextY = 0;
	
	std::vector<std::pair<Widget, float>> m_widgets;
};
