#pragma once

#include "Button.hpp"
#include "ComboBox.hpp"
#include "ToggleButton.hpp"
#include "SubtitleWidget.hpp"
#include "Slider.hpp"
#include "KeyBindingWidget.hpp"

using Widget = std::variant<Button, ComboBox, ToggleButton, KeyBindingWidget, Slider, SubtitleWidget>;

class WidgetList
{
public:
	explicit WidgetList(float width, float widgetSpacing = 10)
		: m_width(width), m_widgetSpacing(widgetSpacing) { }
	
	float Width() const { return m_width; }
	float Height() const { return m_height; }
	float WidgetSpacing() const { return m_widgetSpacing; }
	
	void AddSpacing(float spacing)
	{
		m_nextY += spacing;
	}
	
	size_t AddWidget(Widget widget);
	
	template <typename CallbackTp>
	void AddWidgetFn(CallbackTp fn)
	{
		AddWidget(fn());
	}
	
	bool Update(float dt, bool allowInteraction);
	void Draw(eg::SpriteBatch& spriteBatch) const;
	
	Widget& GetWidget(size_t index) { return m_widgets[index].first; }
	const Widget& GetWidget(size_t index) const { return m_widgets[index].first; }
	
	glm::vec2 position;
	glm::vec2 relativeOffset;
	
	float maxHeight = INFINITY;
	
private:
	float m_width;
	float m_widgetSpacing;
	float m_height = 0;
	float m_nextY = 0;
	
	std::vector<std::pair<Widget, float>> m_widgets;
};
