#include <fstream>
#include "MainMenuGameState.hpp"
#include "Gui/OptionsMenu.hpp"
#include "Levels.hpp"
#include "Gui/GuiCommon.hpp"
#include "MainGameState.hpp"

constexpr float BUTTON_W = 200;

MainMenuGameState* mainMenuGameState;

MainMenuGameState::MainMenuGameState()
	: m_mainWidgetList(BUTTON_W), m_levelHighlightIntensity(levels.size(), 0.0f)
{
	m_mainWidgetList.AddWidget(Button("Play", [&] { m_screen = Screen::LevelSelect; }));
	m_mainWidgetList.AddWidget(Button("Options", [&] { m_screen = Screen::Options; optionsMenuOpen = true; }));
	m_mainWidgetList.AddWidget(Button("Quit", [&] { eg::Close(); }));
	
	m_mainWidgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
	
	for (size_t i = 0; i < levels.size(); i++)
	{
		if (levels[i].isExtra)
			m_extraLevelIds.push_back(i);
	}
	for (std::string_view levelName : levelsOrder)
	{
		int64_t levelIndex = FindLevel(levelName);
		if (levelIndex != -1)
			m_mainLevelIds.push_back(levelIndex);
	}
	
	m_levelSelectBackButton.text = "Back";
	m_levelSelectBackButton.width = 200;
	m_levelSelectBackButton.onClick = [&] { m_screen = Screen::Main; };
}

void MainMenuGameState::RunFrame(float dt)
{
	if (m_levelHighlightIntensity.size() < levels.size())
	{
		m_levelHighlightIntensity.resize(levels.size(), 0.0f);
	}
	if (m_screen != Screen::LevelSelect)
	{
		std::fill(m_levelHighlightIntensity.begin(), m_levelHighlightIntensity.end(), 0.0f);
	}
	
	if (m_screen == Screen::Main)
	{
		m_mainWidgetList.position = glm::vec2(eg::CurrentResolutionX(), eg::CurrentResolutionY()) / 2.0f;
		m_mainWidgetList.Update(dt, m_screen == Screen::Main);
		m_mainWidgetList.Draw(eg::SpriteBatch::overlay);
	}
	else if (m_screen == Screen::Options)
	{
		UpdateOptionsMenu(dt);
		DrawOptionsMenu(eg::SpriteBatch::overlay);
		
		if (!optionsMenuOpen)
		{
			m_screen = Screen::Main;
		}
	}
	else if (m_screen == Screen::LevelSelect)
	{
		DrawLevelSelect(dt);
	}
	
	if (ComboBox::current)
	{
		ComboBox::current->DrawOverlay(eg::SpriteBatch::overlay);
	}
}

void MainMenuGameState::DrawLevelSelect(float dt)
{
	const bool clicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft);
	
	std::vector<int64_t>* levelIds = m_inExtraLevelsTab ? &m_extraLevelIds : &m_mainLevelIds;
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	constexpr int LEVEL_BOX_W = 150;
	constexpr int LEVEL_BOX_H = 120;
	constexpr int SPACING_X = 15;
	constexpr int SPACING_Y = 30;
	constexpr int MARGIN_X = 100;
	constexpr int INFLATE_PIXELS_Y = LEVEL_BOX_H * style::ButtonInflatePercent;
	
	int boxStartY = eg::CurrentResolutionY() - 200;
	int boxEndY = 100;
	int visibleHeight = boxStartY - boxEndY;
	int numPerRow = (eg::CurrentResolutionX() - MARGIN_X * 2 + SPACING_X) / (LEVEL_BOX_W + SPACING_X);
	bool cursorInLevelsArea = eg::Rectangle(MARGIN_X, boxEndY, eg::CurrentResolutionX() - MARGIN_X * 2, visibleHeight).Contains(flippedCursorPos);
	
	const eg::Texture& thumbnailNaTexture = eg::GetAsset<eg::Texture>("Textures/ThumbnailNA.png");
	const eg::Texture& lockTexture = eg::GetAsset<eg::Texture>("Textures/Lock.png");
	
	//Draws the level boxes
	eg::SpriteBatch::overlay.PushScissor(0, boxEndY - INFLATE_PIXELS_Y, eg::CurrentResolutionX(), visibleHeight + INFLATE_PIXELS_Y * 2);
	for (size_t i = 0; i < levelIds->size(); i++)
	{
		const Level& level = levels[levelIds->at(i)];
		
		int gridX = i % numPerRow;
		int gridY = i / numPerRow;
		int x = MARGIN_X + gridX * (LEVEL_BOX_W + SPACING_X);
		float y = (boxStartY - gridY * (LEVEL_BOX_H + SPACING_Y) - LEVEL_BOX_H) + m_levelSelectScroll;
		
		eg::Rectangle rect(x, y, LEVEL_BOX_W, LEVEL_BOX_H);
		bool hovered = cursorInLevelsArea && rect.Contains(flippedCursorPos) && level.status != LevelStatus::Locked;
		if (hovered && clicked)
		{
			std::string levelPath = GetLevelPath(level.name);
			std::ifstream levelStream(levelPath, std::ios::binary);
			SetCurrentGS(mainGameState);
			mainGameState->LoadWorld(levelStream, levelIds->at(i));
		}
		
		float& highlightIntensity = m_levelHighlightIntensity[levelIds->at(i)];
		if (hovered)
			highlightIntensity = std::min(highlightIntensity + dt / style::HoverAnimationTime, 1.0f);
		else
			highlightIntensity = std::max(highlightIntensity - dt / style::HoverAnimationTime, 0.0f);
		
		const eg::Texture* texture = &thumbnailNaTexture;
		if (level.thumbnail.handle != nullptr)
			texture = &level.thumbnail;
		
		float inflate = level.status == LevelStatus::Locked ? 0 : highlightIntensity * style::ButtonInflatePercent;
		glm::vec2 inflateSize = inflate * rect.Size();
		eg::Rectangle inflatedRect(
			rect.x - inflateSize.x, rect.y - inflateSize.y, 
			rect.w + inflateSize.x * 2, rect.h + inflateSize.y * 2);
		
		float shade = glm::mix(0.8f, 1.0f, highlightIntensity);
		if (level.status == LevelStatus::Locked)
			shade *= 0.4f;
		
		eg::SpriteBatch::overlay.Draw(*texture, inflatedRect, eg::ColorLin(1, 1, 1, 1).ScaleRGB(shade), eg::SpriteFlags::None);
		
		if (level.status == LevelStatus::Locked)
		{
			eg::Rectangle lockTextureRect = eg::Rectangle::CreateCentered(inflatedRect.Center(), lockTexture.Width(), lockTexture.Height());
			eg::SpriteBatch::overlay.Draw(lockTexture, lockTextureRect, eg::ColorLin(1, 1, 1, 1), eg::SpriteFlags::None);
		}
	}
	eg::SpriteBatch::overlay.PopScissor();
	
	//Updates scroll
	int totalRows = levelIds->size() / numPerRow + 1;
	int totalHeight = totalRows * LEVEL_BOX_H + (totalRows - 1) * SPACING_Y;
	int maxScroll = std::max(totalHeight - visibleHeight, 0);
	constexpr float MAX_SCROLL_VEL = 1000;
	m_levelSelectScrollVel *= 1 - std::min(dt * 10.0f, 1.0f);
	m_levelSelectScrollVel += (eg::InputState::Previous().scrollY - eg::InputState::Current().scrollY) * MAX_SCROLL_VEL * 0.5f;
	m_levelSelectScrollVel = glm::clamp(m_levelSelectScrollVel, -MAX_SCROLL_VEL, MAX_SCROLL_VEL);
	m_levelSelectScroll = glm::clamp(m_levelSelectScroll + m_levelSelectScrollVel * dt, 0.0f, (float)maxScroll);
	
	//Draws the scroll bar
	if (maxScroll != 0)
	{
		constexpr int SCROLL_BAR_W = 6;
		eg::Rectangle scrollAreaRect(eg::CurrentResolutionX() - MARGIN_X + 5, boxEndY, SCROLL_BAR_W, visibleHeight);
		eg::SpriteBatch::overlay.DrawRect(scrollAreaRect, style::ButtonColorDefault);
		
		float barHeight = scrollAreaRect.h * (float)visibleHeight / (float)totalHeight;
		float barYOffset = (m_levelSelectScroll / (float)maxScroll) * (scrollAreaRect.h - barHeight);
		eg::Rectangle scrollBarRect(scrollAreaRect.x + 1, scrollAreaRect.y + scrollAreaRect.h - barYOffset - barHeight, SCROLL_BAR_W - 2, barHeight);
		eg::SpriteBatch::overlay.DrawRect(scrollBarRect, style::ButtonColorHover);
	}
	
	//Draws and updates tabs
	if (!m_extraLevelIds.empty())
	{
		int tabTextY = boxStartY + 30;
		float tabTextX = MARGIN_X + 10;
		auto DrawAndUpdateTab = [&] (std::string_view text, bool mode)
		{
			glm::vec2 extents;
			float opacity = mode == m_inExtraLevelsTab ? 0.7f : 0.15f;
			eg::SpriteBatch::overlay.DrawText(
				*style::UIFont,
				text,
				glm::vec2(tabTextX, tabTextY),
				eg::ColorLin(eg::Color::White).ScaleAlpha(opacity),
				1, &extents);
			if (eg::Rectangle(tabTextX, tabTextY, extents).Contains(flippedCursorPos) && clicked)
				m_inExtraLevelsTab = mode;
			tabTextX += extents.x + 20;
		};
		
		DrawAndUpdateTab("Main Levels", false);
		DrawAndUpdateTab("Extra Levels", true);
	}
	
	m_levelSelectBackButton.position = glm::vec2(MARGIN_X, boxEndY - 60);
	m_levelSelectBackButton.Update(dt, true);
	m_levelSelectBackButton.Draw(eg::SpriteBatch::overlay);
}
