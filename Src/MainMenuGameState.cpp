#include "MainMenuGameState.hpp"
#include "Gui/OptionsMenu.hpp"
#include "Gui/GuiCommon.hpp"
#include "Levels.hpp"
#include "MainGameState.hpp"
#include "Settings.hpp"

#include <fstream>

constexpr float BUTTON_W = 200;

MainMenuGameState* mainMenuGameState;

static int64_t FindContinueLevel()
{
	for (std::string_view levelName : levelsOrder)
	{
		int64_t level = FindLevel(levelName);
		if (level != -1 && levels[level].status == LevelStatus::Unlocked)
			return level;
	}
	return -1;
}

void MainMenuGameState::ContinueClicked()
{
	int64_t level = FindContinueLevel();
	if (level == -1)
		return;
	if (std::unique_ptr<World> world = LoadLevelWorld(levels[level], false))
	{
		SetCurrentGS(mainGameState);
		mainGameState->SetWorld(std::move(world), level);
	}
}

MainMenuGameState::MainMenuGameState()
	: m_mainWidgetList(BUTTON_W), m_levelHighlightIntensity(levels.size(), 0.0f),
	  m_worldBlurRenderer(2, eg::Format::R8G8B8A8_UNorm)
{
	int64_t continueLevel = FindContinueLevel();
	if (continueLevel != -1)
	{
		backgroundLevel = &levels[continueLevel];
	}
	
	m_mainWidgetList.AddWidget(Button("Continue", [&] { ContinueClicked(); }));
	m_mainWidgetList.AddWidget(Button("Select Level", [&] { m_screen = Screen::LevelSelect; }));
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
	
#ifdef BUILD_DATE
#ifdef NDEBUG
	m_versionString = BUILD_DATE " opt";
#else
	m_versionString = BUILD_DATE " debug";
#endif
#endif
}

void MainMenuGameState::RunFrame(float dt)
{
	int64_t continueLevel = FindContinueLevel();
	Button& continueButton = std::get<Button>(m_mainWidgetList.GetWidget(0));
	if (continueLevel != -1 && levels[continueLevel].name == levelsOrder[0])
		continueButton.text = "Start Game";
	else
		continueButton.text = "Continue";
	continueButton.enabled = continueLevel != -1;
	
	m_spriteBatch.Begin();
	
	RenderWorld(dt);
	
	if (settings.keyMenu.IsDown() && !settings.keyMenu.WasDown() && !KeyBindingWidget::anyKeyBindingPickingKey)
	{
		m_screen = Screen::Main;
	}
	
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
		m_mainWidgetList.Draw(m_spriteBatch);
	}
	else if (m_screen == Screen::Options)
	{
		UpdateOptionsMenu(dt);
		DrawOptionsMenu(m_spriteBatch);
		
		if (!optionsMenuOpen)
		{
			m_screen = Screen::Main;
		}
	}
	else if (m_screen == Screen::LevelSelect)
	{
		DrawLevelSelect(dt);
	}
	
	if (!m_versionString.empty())
	{
		m_spriteBatch.DrawText(*style::UIFont, m_versionString, glm::vec2(5, 5), eg::ColorLin(1, 1, 1, 0.5f),
						 0.5f, nullptr, eg::TextFlags::DropShadow);
	}
	
	if (ComboBox::current)
	{
		ComboBox::current->DrawOverlay(m_spriteBatch);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	m_spriteBatch.End(eg::CurrentResolutionX(), eg::CurrentResolutionY(), rpBeginInfo);
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
	const eg::Texture& completedTexture = eg::GetAsset<eg::Texture>("Textures/LevelCompleted.png");
	
	//Draws the level boxes
	m_spriteBatch.PushScissor(0, boxEndY - INFLATE_PIXELS_Y, eg::CurrentResolutionX(), visibleHeight + INFLATE_PIXELS_Y * 2);
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
			if (std::unique_ptr<World> world = LoadLevelWorld(level, false))
			{
				SetCurrentGS(mainGameState);
				mainGameState->SetWorld(std::move(world), levelIds->at(i));
			}
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
		
		m_spriteBatch.Draw(*texture, inflatedRect, eg::ColorLin(1, 1, 1, 1).ScaleRGB(shade));
		
		if (level.status == LevelStatus::Locked)
		{
			eg::Rectangle lockTextureRect = eg::Rectangle::CreateCentered(inflatedRect.Center(), lockTexture.Width(), lockTexture.Height());
			m_spriteBatch.Draw(lockTexture, lockTextureRect, eg::ColorLin(1, 1, 1, 1));
		}
		else if (level.status == LevelStatus::Completed)
		{
			glm::vec2 texSize = glm::vec2(completedTexture.Width(), completedTexture.Height()) * (1 + inflate);
			eg::Rectangle completeddTextureRect = eg::Rectangle(inflatedRect.Max() - texSize - 5.0f, texSize);
			m_spriteBatch.Draw(completedTexture, completeddTextureRect, eg::ColorLin(1, 1, 1, shade));
		}
	}
	m_spriteBatch.PopScissor();
	
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
		m_spriteBatch.DrawRect(scrollAreaRect, style::ButtonColorDefault);
		
		float barHeight = scrollAreaRect.h * (float)visibleHeight / (float)totalHeight;
		float barYOffset = (m_levelSelectScroll / (float)maxScroll) * (scrollAreaRect.h - barHeight);
		eg::Rectangle scrollBarRect(scrollAreaRect.x + 1, scrollAreaRect.y + scrollAreaRect.h - barYOffset - barHeight, SCROLL_BAR_W - 2, barHeight);
		m_spriteBatch.DrawRect(scrollBarRect, style::ButtonColorHover);
	}
	
	//Draws and updates tabs
	if (settings.showExtraLevels && !m_extraLevelIds.empty())
	{
		int tabTextY = boxStartY + 30;
		float tabTextX = MARGIN_X + 10;
		auto DrawAndUpdateTab = [&] (std::string_view text, bool mode)
		{
			glm::vec2 extents;
			float opacity = mode == m_inExtraLevelsTab ? 0.7f : 0.15f;
			m_spriteBatch.DrawText(
				*style::UIFont,
				text,
				glm::vec2(tabTextX, tabTextY),
				eg::ColorLin(eg::Color::White).ScaleAlpha(opacity),
				1, &extents, eg::TextFlags::DropShadow);
			if (eg::Rectangle(tabTextX, tabTextY, extents).Contains(flippedCursorPos) && clicked)
				m_inExtraLevelsTab = mode;
			tabTextX += extents.x + 20;
		};
		
		DrawAndUpdateTab("Main Levels", false);
		DrawAndUpdateTab("Extra Levels", true);
	}
	
	m_levelSelectBackButton.position = glm::vec2(MARGIN_X, boxEndY - 60);
	m_levelSelectBackButton.Update(dt, true);
	m_levelSelectBackButton.Draw(m_spriteBatch);
}

float* menuWorldColorScale = eg::TweakVarFloat("menu_world_color_scale", 0.6f, 0.0f, 1.0f);

void MainMenuGameState::RenderWorld(float dt)
{
	if (m_world == nullptr)
	{
		if (backgroundLevel == nullptr)
			return;
		m_world = LoadLevelWorld(*backgroundLevel, false);
		m_physicsEngine = {};
		m_worldGameTime = 0;
		GameRenderer::instance->WorldChanged(*m_world);
	}
	
	eg::Format inputFormat = eg::Format::R8G8B8A8_UNorm;
	if (m_worldRenderTexture.handle == nullptr || eg::CurrentResolutionX() != (int)m_worldRenderTexture.Width() ||
		eg::CurrentResolutionY() != (int)m_worldRenderTexture.Height() || inputFormat != m_worldRenderTexture.Format())
	{
		eg::TextureCreateInfo textureCI;
		textureCI.width = eg::CurrentResolutionX();
		textureCI.height = eg::CurrentResolutionY();
		textureCI.mipLevels = 1;
		textureCI.format = inputFormat;
		textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment;
		m_worldRenderTexture = eg::Texture::Create2D(textureCI);
		
		eg::FramebufferAttachment attachment(m_worldRenderTexture.handle);
		m_worldRenderFramebuffer = eg::Framebuffer({ &attachment, 1 });
	}
	m_worldBlurRenderer.MaybeUpdateResolution(eg::CurrentResolutionX(), eg::CurrentResolutionY());   
	
	m_worldGameTime += dt;
	
	GameRenderer::instance->m_player = nullptr;
	GameRenderer::instance->m_physicsDebugRenderer = nullptr;
	GameRenderer::instance->m_physicsEngine = &m_physicsEngine;
	GameRenderer::instance->m_particleManager = nullptr;
	GameRenderer::instance->m_gravityGun = nullptr;
	GameRenderer::instance->postColorScale = *menuWorldColorScale;
	GameRenderer::instance->SetViewMatrixFromThumbnailCamera(*m_world);
	
	WorldUpdateArgs updateArgs = { };
	updateArgs.mode = WorldMode::Menu;
	updateArgs.dt = dt;
	updateArgs.world = m_world.get();
	updateArgs.waterSim = &GameRenderer::instance->m_waterSimulator;
	updateArgs.physicsEngine = &m_physicsEngine;
	updateArgs.invalidateShadows = [this] (const eg::Sphere& sphere) { GameRenderer::instance->InvalidateShadows(sphere); };
	
	m_physicsEngine.BeginCollect();
	m_world->CollectPhysicsObjects(m_physicsEngine, dt);
	m_physicsEngine.EndCollect();
	
	m_world->Update(updateArgs, &m_physicsEngine);
	
	{
		auto waterUpdateTimer = eg::StartCPUTimer("Water Update MT");
		GameRenderer::instance->m_waterSimulator.Update(*m_world, false);
	}
	
	GameRenderer::instance->Render(*m_world, m_worldGameTime, dt, m_worldRenderFramebuffer.handle,
	                               eg::CurrentResolutionX(), eg::CurrentResolutionY());
	
	m_worldRenderTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	m_worldBlurRenderer.Render(m_worldRenderTexture);
	
	eg::Rectangle dstRect;
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan)
		dstRect = eg::Rectangle(0, 0, eg::CurrentResolutionX(), eg::CurrentResolutionY());
	else
		dstRect = eg::Rectangle(0, eg::CurrentResolutionY(), eg::CurrentResolutionX(), -eg::CurrentResolutionY());
	
	m_spriteBatch.Draw(m_worldBlurRenderer.OutputTexture(), dstRect, eg::ColorLin(1, 1, 1, 1), eg::SpriteFlags::ForceLowestMipLevel);
}

void MainMenuGameState::OnDeactivate()
{
	GameRenderer::instance->m_waterSimulator.Stop();
	m_physicsEngine = {};
	m_world = {};
}
