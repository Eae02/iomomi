#include "WidgetList.hpp"

static constexpr float WIDGET_SPACING = 15;

void WidgetList::AddWidget(Widget widget)
{
	float y = m_nextY;
	std::visit([&] (auto& widgetVis) {
		widgetVis.width = m_width;
		y += widgetVis.height;
		m_nextY += widgetVis.height + WIDGET_SPACING;
	}, widget);
	m_widgets.emplace_back(std::move(widget), y);
	m_height = m_nextY - WIDGET_SPACING;
}

bool WidgetList::Update(float dt, bool allowInteraction)
{
	glm::vec2 pos = position + relativeOffset * glm::vec2(m_width, m_height);
	bool noComboBox = ComboBox::current == nullptr;
	
	for (auto& widgetPair : m_widgets)
	{
		std::visit([&] (auto& widget)
		{
			widget.position = pos;
			widget.position.y -= widgetPair.second;
			
			widget.Update(dt, allowInteraction && (noComboBox || (void*)ComboBox::current == (void*)&widget));
		}, widgetPair.first);
	}
	return allowInteraction;
}

void WidgetList::Draw(eg::SpriteBatch& spriteBatch) const
{
	for (const auto& widgetPair : m_widgets)
	{
		std::visit([&] (const auto& widget)
		{
			widget.Draw(spriteBatch);
		}, widgetPair.first);
	}
}
