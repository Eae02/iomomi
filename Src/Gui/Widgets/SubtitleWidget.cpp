#include "SubtitleWidget.hpp"

#include "../GuiCommon.hpp"

void SubtitleWidget::Draw(const GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch) const
{
	constexpr float TEXT_SCALE = 0.8f;

	glm::vec2 textExt = style::UIFont->GetTextExtents(m_text) * TEXT_SCALE;
	glm::vec2 textPos(position.x + (width - textExt.x) / 2.0f, position.y + 10);
	spriteBatch.DrawText(
		*style::UIFont, m_text, textPos, eg::ColorLin(eg::Color::White), TEXT_SCALE, nullptr,
		eg::TextFlags::DropShadow);

	spriteBatch.DrawLine(position, position + glm::vec2(width, 0), eg::ColorLin(eg::Color::White), 1);
}
