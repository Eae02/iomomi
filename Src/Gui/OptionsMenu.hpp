#pragma once

extern bool optionsMenuOpen;

void InitOptionsMenu();
void UpdateOptionsMenu(float dt, const glm::vec2& positionOffset, bool allowInteraction = true);
void DrawOptionsMenu(eg::SpriteBatch& spriteBatch);
