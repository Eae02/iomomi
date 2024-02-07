#pragma once

#include "GuiCommon.hpp"

extern bool optionsMenuOpen;

void InitOptionsMenu();
void UpdateOptionsMenu(const GuiFrameArgs& frameArgs, const glm::vec2& positionOffset, bool allowInteraction = true);
void DrawOptionsMenu(const GuiFrameArgs& frameArgs, eg::SpriteBatch& spriteBatch);
