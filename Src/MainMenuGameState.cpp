#include "MainMenuGameState.hpp"
#include "Gui/OptionsMenu.hpp"
#include "Gui/GuiCommon.hpp"
#include "Levels.hpp"
#include "MainGameState.hpp"
#include "Settings.hpp"
#include "ThumbnailRenderer.hpp"

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
	for (std::string_view levelName : levelsOrder)
	{
		int64_t levelIndex = FindLevel(levelName);
		if (levelIndex != -1)
			m_levelIds.push_back(levelIndex);
	}
	m_numMainLevels = m_levelIds.size();
	for (size_t i = 0; i < levels.size(); i++)
	{
		if (levels[i].isExtra)
			m_levelIds.push_back(i);
	}
	
	int64_t continueLevel = FindContinueLevel();
	if (continueLevel != -1)
	{
		backgroundLevel = &levels[continueLevel];
	}
	else
	{
		backgroundLevel = &levels[m_levelIds[time(nullptr) % m_numMainLevels]];
	}
	
	m_mainWidgetList.AddWidget(Button("Continue", [&] { ContinueClicked(); }));
	m_mainWidgetList.AddWidget(Button("Select Level", [&]
	{
		m_screen = Screen::LevelSelect;
		m_transitionToMainScreen = false;
	}));
	m_mainWidgetList.AddWidget(Button("Options", [&]
	{
		m_screen = Screen::Options;
		optionsMenuOpen = true;
		m_transitionToMainScreen = false;
	}));
	m_mainWidgetList.AddWidget(Button("Quit", [&] { eg::Close(); }));
	
	m_mainWidgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
	
	m_levelSelectBackButton.text = "Back";
	m_levelSelectBackButton.width = 200;
	m_levelSelectBackButton.onClick = [&] { m_transitionToMainScreen = true; };
}

static constexpr float TRANSITION_TIME = 0.1f;
static constexpr float TRANSITION_SLIDE_DIST = 40;

void MainMenuGameState::RunFrame(float dt)
{
	if (m_lastFrameIndex + 1 != eg::FrameIdx())
	{
		m_transitionToMainScreen = true;
		m_screen = Screen::Main;
		m_transitionProgress = 0;
	}
	m_lastFrameIndex = eg::FrameIdx();
		
	int64_t continueLevel = FindContinueLevel();
	Button& continueButton = std::get<Button>(m_mainWidgetList.GetWidget(0));
	if (continueLevel != -1 && levels[continueLevel].name == levelsOrder[0])
		continueButton.text = "Start Game";
	else
		continueButton.text = "Continue";
	continueButton.enabled = continueLevel != -1;
	
	m_spriteBatch.Begin();
	
	RenderWorld(dt);
	
	if (m_levelHighlightIntensity.size() < levels.size())
	{
		m_levelHighlightIntensity.resize(levels.size(), 0.0f);
	}
	if (m_screen != Screen::LevelSelect)
	{
		std::fill(m_levelHighlightIntensity.begin(), m_levelHighlightIntensity.end(), 0.0f);
	}
	
	AnimateProperty(m_transitionProgress, dt, TRANSITION_TIME, !m_transitionToMainScreen);
	float transitionSmooth = glm::smoothstep(0.0f, 1.0f, m_transitionProgress);
	
	if (m_transitionProgress == 0 && m_screen != Screen::Main)
		m_screen = Screen::Main;
	
	m_mainWidgetList.position.x = eg::CurrentResolutionX() / 2.0f - transitionSmooth * TRANSITION_SLIDE_DIST;
	m_mainWidgetList.position.y = eg::CurrentResolutionY() / 2.0f;
	if (transitionSmooth < 1)
	{
		m_mainWidgetList.Update(dt, transitionSmooth == 0);
		m_spriteBatch.opacityScale = 1 - transitionSmooth;
		m_mainWidgetList.Draw(m_spriteBatch);
		m_spriteBatch.opacityScale = 1;
	}
	
	if (m_screen != Screen::Main)
	{
		const float invXOffset = TRANSITION_SLIDE_DIST - transitionSmooth * TRANSITION_SLIDE_DIST;
		m_spriteBatch.opacityScale = transitionSmooth;
		
		m_spriteBatch.DrawRect(
			eg::Rectangle(0, 0, eg::CurrentResolutionX(), eg::CurrentResolutionY()),
			eg::ColorLin(0, 0, 0, 0.5f));
		
		if (m_screen == Screen::Options)
		{
			UpdateOptionsMenu(dt, glm::vec2(invXOffset, 0), m_transitionProgress == 1);
			DrawOptionsMenu(m_spriteBatch);
			
			if (!optionsMenuOpen)
			{
				m_transitionToMainScreen = true;
			}
		}
		else if (m_screen == Screen::LevelSelect)
		{
			DrawLevelSelect(dt, invXOffset);
		}
		m_spriteBatch.opacityScale = 1;
		
		if (settings.keyMenu.IsDown() && !settings.keyMenu.WasDown() && !KeyBindingWidget::anyKeyBindingPickingKey)
		{
			m_screen = Screen::Main;
			m_transitionToMainScreen = true;
		}
	}
	
#if !defined(NDEBUG)
	std::string_view infoLine = "debug";
#elif defined(BUILD_ID)
	std::string_view infoLine = BUILD_ID " " BUILD_DATE;
#else
	std::string_view infoLine = BUILD_DATE;
#endif
	m_spriteBatch.DrawText(*style::UIFontSmall, infoLine, glm::vec2(5), eg::ColorLin(1, 1, 1, 0.2f), 1,
	                       nullptr, eg::TextFlags::DropShadow);
	
	if (ComboBox::current)
	{
		ComboBox::current->DrawOverlay(m_spriteBatch);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	m_spriteBatch.End(eg::CurrentResolutionX(), eg::CurrentResolutionY(), rpBeginInfo);
}

void MainMenuGameState::DrawLevelSelect(float dt, float xOffset)
{
	const bool clicked = eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft) && xOffset == 0;
	
	glm::vec2 flippedCursorPos(eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY());
	
	const eg::Texture& thumbnailNaTexture = eg::GetAsset<eg::Texture>("Textures/ThumbnailNA.png");
	const eg::Texture& lockTexture = eg::GetAsset<eg::Texture>("Textures/Lock.png");
	const eg::Texture& completedTexture = eg::GetAsset<eg::Texture>("Textures/LevelCompleted.png");
	const eg::Texture& titleTexture = eg::GetAsset<eg::Texture>("Textures/LevelSelect.png");
	
	const float marginX = 0.05f * eg::CurrentResolutionX();
	
	const float titleTextureWidth = std::min(eg::CurrentResolutionX() * 0.25f, (float)titleTexture.Width());
	const float titleTextureHeight = titleTextureWidth * titleTexture.Height() / titleTexture.Width();
	const eg::Rectangle titleRect(marginX + xOffset, eg::CurrentResolutionY() - 75 - titleTextureHeight, titleTextureWidth, titleTextureHeight);
	m_spriteBatch.Draw(titleTexture, titleRect, eg::ColorLin(1, 1, 1, 1));
	
	const float levelBoxW = glm::clamp(eg::CurrentResolutionX() * 0.1f, 150.0f, (float)LEVEL_THUMBNAIL_RES_X);
	const float levelBoxH = 0.8f * levelBoxW;
	const float levelBoxSpacingX = levelBoxW * 0.1f;
	const float levelBoxSpacingY = levelBoxSpacingX * 2;
	const float inflatePixelsY = levelBoxH * style::ButtonInflatePercent;
	
	const float boxStartY = eg::CurrentResolutionY() - 100 - titleTextureHeight;
	const float boxEndY = 100;
	const float visibleHeight = boxStartY - boxEndY;
	const int numPerRow = (eg::CurrentResolutionX() - marginX * 2 + levelBoxSpacingX) / (levelBoxW + levelBoxSpacingX);
	const bool cursorInLevelsArea = eg::Rectangle(marginX, boxEndY, eg::CurrentResolutionX() - marginX * 2, visibleHeight).Contains(flippedCursorPos);
	
	float totalHeight = 0;
	
	//Draws the level boxes
	size_t numLevels = settings.showExtraLevels ? m_levelIds.size() : m_numMainLevels;
	m_spriteBatch.PushScissor(0, boxEndY - inflatePixelsY, eg::CurrentResolutionX(), visibleHeight + inflatePixelsY * 2);
	for (size_t i = 0; i < numLevels; i++)
	{
		const Level& level = levels[m_levelIds[i]];
		
		size_t iForGridCalculation = i >= m_numMainLevels ? i - m_numMainLevels : i;
		const float gridX = iForGridCalculation % numPerRow;
		const float gridY = iForGridCalculation / numPerRow;
		const float x = marginX + gridX * (levelBoxW + levelBoxSpacingX) + xOffset;
		float yNoScroll = gridY * (levelBoxH + levelBoxSpacingY) + levelBoxH;
		if (i >= m_numMainLevels)
		{
			yNoScroll += (levelBoxH + levelBoxSpacingY) * ((m_numMainLevels + numPerRow - 1) / numPerRow) + 40;
		}
		
		totalHeight = std::max(yNoScroll, totalHeight);
		const float y = boxStartY - yNoScroll + m_levelSelectScroll;
		if (i == m_numMainLevels)
		{
			m_spriteBatch.DrawText(*style::UIFont, "Extra Levels", glm::vec2(x, y + levelBoxH + 20),
						  eg::ColorLin(1, 1, 1, 0.75f), 1, nullptr, eg::TextFlags::DropShadow);
			m_spriteBatch.DrawLine(glm::vec2(x, y + levelBoxH + 15),
						  glm::vec2(eg::CurrentResolutionX() - marginX + xOffset, y + levelBoxH + 15),
						  eg::ColorLin(1, 1, 1, 0.75f));
		}
		
		eg::Rectangle rect(x, y, levelBoxW, levelBoxH);
		bool hovered = cursorInLevelsArea && rect.Contains(flippedCursorPos) && level.status != LevelStatus::Locked && xOffset == 0;
		if (hovered && clicked)
		{
			if (std::unique_ptr<World> world = LoadLevelWorld(level, false))
			{
				SetCurrentGS(mainGameState);
				mainGameState->SetWorld(std::move(world), m_levelIds[i]);
			}
		}
		
		float& highlightIntensity = m_levelHighlightIntensity[m_levelIds[i]];
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
		
		if (eg::DevMode())
		{
			float nameLen = style::UIFontSmall->GetTextExtents(level.name).x;
			m_spriteBatch.DrawText(*style::UIFontSmall, level.name,
			                       glm::vec2(rect.CenterX() - nameLen / 2, inflatedRect.y + 10),
			                       eg::ColorLin(1, 1, 1, glm::mix(0.4f, 1.0f, highlightIntensity)),
			                       1, nullptr, eg::TextFlags::DropShadow);
		}
		
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
	const float maxScroll = std::max(totalHeight - visibleHeight, 0.0f);
	constexpr float SCROLL_SPEED = 1500;
	m_levelSelectScrollVel *= 1 - std::min(dt * 10.0f, 1.0f);
	m_levelSelectScrollVel += (eg::InputState::Previous().scrollY - eg::InputState::Current().scrollY) * SCROLL_SPEED * 0.5f;
	m_levelSelectScrollVel = glm::clamp(m_levelSelectScrollVel, -SCROLL_SPEED, SCROLL_SPEED);
	m_levelSelectScroll = glm::clamp(m_levelSelectScroll + m_levelSelectScrollVel * dt, 0.0f, (float)maxScroll);
	
	//Draws the scroll bar
	if (maxScroll > 0)
	{
		constexpr int SCROLL_BAR_W = 6;
		eg::Rectangle scrollAreaRect(eg::CurrentResolutionX() - marginX + 5 + xOffset, boxEndY, SCROLL_BAR_W, visibleHeight);
		m_spriteBatch.DrawRect(scrollAreaRect, style::ButtonColorDefault);
		
		float barHeight = scrollAreaRect.h * (float)visibleHeight / (float)totalHeight;
		float barYOffset = (m_levelSelectScroll / (float)maxScroll) * (scrollAreaRect.h - barHeight);
		eg::Rectangle scrollBarRect(scrollAreaRect.x + 1, scrollAreaRect.y + scrollAreaRect.h - barYOffset - barHeight, SCROLL_BAR_W - 2, barHeight);
		m_spriteBatch.DrawRect(scrollBarRect, style::ButtonColorHover);
	}
	
	m_levelSelectBackButton.position = glm::vec2(marginX, boxEndY - 60);
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
	updateArgs.plShadowMapper = &GameRenderer::instance->m_plShadowMapper;
	
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
