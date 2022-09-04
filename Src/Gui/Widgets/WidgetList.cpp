#include "WidgetList.hpp"

size_t WidgetList::AddWidget(Widget widget)
{
	float y = m_nextY;
	std::visit([&] (auto& widgetVis) {
		widgetVis.width = m_width;
		y += widgetVis.height;
		m_nextY += widgetVis.height + m_widgetSpacing;
	}, widget);
	m_widgets.emplace_back(std::move(widget), y);
	m_height = m_nextY - m_widgetSpacing;
	return eg::ToInt64(m_widgets.size()) - 1;
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
