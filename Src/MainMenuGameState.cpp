#include "MainMenuGameState.hpp"

#include <fstream>

#include "AudioPlayers.hpp"
#include "Gui/GuiCommon.hpp"
#include "Gui/OptionsMenu.hpp"
#include "Levels.hpp"
#include "MainGameState.hpp"
#include "Settings.hpp"
#include "ThumbnailRenderer.hpp"

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
	m_mainWidgetList.AddWidget(Button(
		"Select Level",
		[&]
		{
			m_screen = Screen::LevelSelect;
			m_transitionToMainScreen = false;
		}
	));
	m_mainWidgetList.AddWidget(Button(
		"Options",
		[&]
		{
			m_screen = Screen::Options;
			optionsMenuOpen = true;
			m_transitionToMainScreen = false;
		}
	));
#ifndef __EMSCRIPTEN__
	m_mainWidgetList.AddWidget(Button("Quit", [&] { eg::Close(); }));
#endif

	m_mainWidgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);

	m_levelSelectBackButton.text = "Back";
	m_levelSelectBackButton.width = 200;
	m_levelSelectBackButton.onClick = [&] { m_transitionToMainScreen = true; };
}

static constexpr float TRANSITION_TIME = 0.1f;
static constexpr float TRANSITION_SLIDE_DIST = 40;

extern bool audioInitializationFailed;

void MainMenuGameState::RunFrame(float dt)
{
	if (m_lastFrameIndex + 1 != eg::FrameIdx())
	{
		m_transitionToMainScreen = true;
		m_screen = Screen::Main;
		m_transitionProgress = 0;
	}
	m_lastFrameIndex = eg::FrameIdx();

	m_currentFrameArgs = GuiFrameArgs::MakeForCurrentFrame(dt);

	int64_t continueLevel = FindContinueLevel();
	Button& continueButton = std::get<Button>(m_mainWidgetList.GetWidget(0));
	if (continueLevel != -1 && levels[continueLevel].name == levelsOrder[0])
		continueButton.text = "Start Game";
	else
		continueButton.text = "Continue";
	continueButton.enabled = continueLevel != -1;

	m_spriteBatch.Reset();

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

	m_mainWidgetList.position.x = m_currentFrameArgs.canvasWidth / 2.0f - transitionSmooth * TRANSITION_SLIDE_DIST;
	m_mainWidgetList.position.y = m_currentFrameArgs.canvasHeight / 2.0f;
	if (transitionSmooth < 1)
	{
		m_mainWidgetList.Update(m_currentFrameArgs, transitionSmooth == 0);
		m_spriteBatch.opacityScale = 1 - transitionSmooth;
		m_mainWidgetList.Draw(m_currentFrameArgs, m_spriteBatch);
		m_spriteBatch.opacityScale = 1;
	}

	if (m_screen != Screen::Main)
	{
		const float invXOffset = TRANSITION_SLIDE_DIST - transitionSmooth * TRANSITION_SLIDE_DIST;
		m_spriteBatch.opacityScale = transitionSmooth;

		m_spriteBatch.DrawRect(
			eg::Rectangle(0, 0, m_currentFrameArgs.canvasWidth, m_currentFrameArgs.canvasHeight),
			eg::ColorLin(0, 0, 0, 0.5f)
		);

		if (m_screen == Screen::Options)
		{
			UpdateOptionsMenu(m_currentFrameArgs, glm::vec2(invXOffset, 0), m_transitionProgress == 1);
			DrawOptionsMenu(m_currentFrameArgs, m_spriteBatch);

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
	std::string infoLine = "Iomomi debug build";
#elif defined(BUILD_ID)
	std::string infoLine = "Iomomi " BUILD_ID " compiled " BUILD_DATE;
#else
	std::string infoLine = "Iomomi compiled " BUILD_DATE;
#endif
	infoLine +=
		eg::Concat({ "\nGfx: ", eg::GetGraphicsDeviceInfo().apiName, " on ", eg::GetGraphicsDeviceInfo().deviceName });

	m_spriteBatch.DrawTextMultiline(
		*style::UIFontSmall, infoLine, glm::vec2(5.0f, 20.0f), eg::ColorLin(1, 1, 1, 0.2f), 1, 0, nullptr,
		eg::TextFlags::DropShadow
	);

	if (audioInitializationFailed)
	{
		std::string_view audioInfoText = "Audio system failed to initialize, no sounds will be played.";
		float audioInfoTextW = style::UIFontSmall->GetTextExtents(audioInfoText).x;
		m_spriteBatch.DrawText(
			*style::UIFontSmall, audioInfoText, glm::vec2(m_currentFrameArgs.canvasWidth - 5.0f - audioInfoTextW, 5),
			eg::ColorSRGB::FromHex(0xff007f).ScaleAlpha(0.6f), 1, nullptr, eg::TextFlags::DropShadow
		);
	}

	if (ComboBox::current)
	{
		ComboBox::current->DrawOverlay(m_currentFrameArgs, m_spriteBatch);
	}

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
	rpBeginInfo.colorAttachments[0].clearValue = eg::ColorSRGB::FromHex(0x041626);
	m_spriteBatch.UploadAndRender(
		eg::SpriteBatch::RenderArgs{
			.framebufferFormat = eg::ColorAndDepthFormat(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil),
		},
		rpBeginInfo, m_currentFrameArgs.GetMatrixToNDC()
	);
}

void MainMenuGameState::DrawLevelSelect(float dt, float xOffset)
{
	const bool clicked =
		eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft) && xOffset == 0;

	const eg::Texture& thumbnailNaTexture = eg::GetAsset<eg::Texture>("Textures/UI/ThumbnailNA.png");
	const eg::Texture& loadingLevelTexture = eg::GetAsset<eg::Texture>("Textures/UI/LoadingLevel.png");
	const eg::Texture& lockTexture = eg::GetAsset<eg::Texture>("Textures/UI/Lock.png");
	const eg::Texture& completedTexture = eg::GetAsset<eg::Texture>("Textures/UI/LevelCompleted.png");
	const eg::Texture& titleTexture = eg::GetAsset<eg::Texture>("Textures/UI/LevelSelect.png");

	const float levelBoxW =
		glm::clamp(m_currentFrameArgs.canvasWidth * 0.1f, 150.0f, static_cast<float>(LEVEL_THUMBNAIL_RES_X));
	const float levelBoxH = 0.8f * levelBoxW;
	const float levelBoxSpacingX = levelBoxW * 0.1f;
	const float levelBoxSpacingY = levelBoxSpacingX * 2;
	const float inflatePixelsY = levelBoxH * style::ButtonInflatePercent;

	const float marginX = 0.05f * m_currentFrameArgs.canvasWidth;
	const int numPerRow = static_cast<int>(std::floor(
		(m_currentFrameArgs.canvasWidth - marginX * 2.0f + levelBoxSpacingX) / (levelBoxW + levelBoxSpacingX)
	));

	const float boxW = numPerRow * (levelBoxW + levelBoxSpacingX) - levelBoxSpacingX;
	const float boxLX = (m_currentFrameArgs.canvasWidth - boxW) / 2;

	const float titleTextureWidth = std::min(m_currentFrameArgs.canvasWidth * 0.25f, (float)titleTexture.Width());
	const float titleTextureHeight = titleTextureWidth * titleTexture.Height() / titleTexture.Width();
	const eg::Rectangle titleRect(
		boxLX + xOffset, m_currentFrameArgs.canvasHeight - 75.0f - titleTextureHeight, titleTextureWidth,
		titleTextureHeight
	);
	m_spriteBatch.Draw(titleTexture, titleRect, eg::ColorLin(1, 1, 1, 1));

	const float boxStartY = m_currentFrameArgs.canvasHeight - 100.0f - titleTextureHeight;
	const float boxEndY = 100;
	const float visibleHeight = boxStartY - boxEndY;
	const bool cursorInLevelsArea =
		eg::Rectangle(boxLX, boxEndY, boxW, visibleHeight).Contains(m_currentFrameArgs.cursorPos);

	float totalHeight = 0;

	// Draws the level boxes
	size_t numLevels = settings.showExtraLevels ? m_levelIds.size() : m_numMainLevels;
	m_spriteBatch.PushScissorF(
		0.0f, (boxEndY - inflatePixelsY) * m_currentFrameArgs.scaleToScreenCoordinates,
		static_cast<float>(eg::CurrentResolutionX()),
		(visibleHeight + inflatePixelsY * 2) * m_currentFrameArgs.scaleToScreenCoordinates
	);
	for (size_t i = 0; i < numLevels; i++)
	{
		const Level& level = levels[m_levelIds[i]];

		const size_t iForGridCalculation = i >= m_numMainLevels ? i - m_numMainLevels : i;
		const float gridX = static_cast<float>(iForGridCalculation % numPerRow);
		const float gridY = static_cast<float>(iForGridCalculation / numPerRow);
		const float x = boxLX + gridX * (levelBoxW + levelBoxSpacingX) + xOffset;
		float yNoScroll = gridY * (levelBoxH + levelBoxSpacingY) + levelBoxH;
		if (i >= m_numMainLevels)
		{
			yNoScroll += (levelBoxH + levelBoxSpacingY) * ((m_numMainLevels + numPerRow - 1) / numPerRow) + 40;
		}

		totalHeight = std::max(yNoScroll, totalHeight);
		const float y = boxStartY - yNoScroll + m_levelSelectScroll.scroll;
		if (i == m_numMainLevels)
		{
			m_spriteBatch.DrawText(
				*style::UIFont, "Extra Levels", glm::vec2(x, y + levelBoxH + 20), eg::ColorLin(1, 1, 1, 0.75f), 1,
				nullptr, eg::TextFlags::DropShadow
			);
			m_spriteBatch.DrawLine(
				glm::vec2(x, y + levelBoxH + 15),
				glm::vec2(eg::CurrentResolutionX() - boxLX + xOffset, y + levelBoxH + 15), eg::ColorLin(1, 1, 1, 0.75f)
			);
		}

		const bool loadingComplete = IsLevelLoadingComplete(level.name);
		const bool canInteract = level.status != LevelStatus::Locked && xOffset == 0 && loadingComplete;

		eg::Rectangle rect(x, y, levelBoxW, levelBoxH);
		const bool hovered = cursorInLevelsArea && rect.Contains(m_currentFrameArgs.cursorPos) && canInteract;
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
		{
			if (highlightIntensity == 0)
				PlayButtonHoverSound();
			highlightIntensity = std::min(highlightIntensity + dt / style::HoverAnimationTime, 1.0f);
		}
		else
		{
			highlightIntensity = std::max(highlightIntensity - dt / style::HoverAnimationTime, 0.0f);
		}

		const eg::Texture* texture = &thumbnailNaTexture;
		if (!loadingComplete)
			texture = &loadingLevelTexture;
		else if (level.thumbnail.handle != nullptr)
			texture = &level.thumbnail;

		float inflate = level.status == LevelStatus::Locked ? 0 : highlightIntensity * style::ButtonInflatePercent;
		glm::vec2 inflateSize = inflate * rect.Size();
		eg::Rectangle inflatedRect(
			rect.x - inflateSize.x, rect.y - inflateSize.y, rect.w + inflateSize.x * 2, rect.h + inflateSize.y * 2
		);

		float shade = glm::mix(0.8f, 1.0f, highlightIntensity);
		if (level.status == LevelStatus::Locked)
			shade *= 0.4f;

		m_spriteBatch.Draw(*texture, inflatedRect, eg::ColorLin(1, 1, 1, 1).ScaleRGB(shade));

		if (eg::DevMode() || level.isExtra)
		{
			float nameLen = style::UIFontSmall->GetTextExtents(level.name).x;
			m_spriteBatch.DrawText(
				*style::UIFontSmall, level.name, glm::vec2(rect.CenterX() - nameLen / 2, inflatedRect.y + 10),
				eg::ColorLin(1, 1, 1, glm::mix(0.4f, 1.0f, highlightIntensity)), 1, nullptr, eg::TextFlags::DropShadow
			);
		}

		if (level.status == LevelStatus::Locked)
		{
			m_spriteBatch.Draw(
				lockTexture,
				eg::Rectangle::CreateCentered(inflatedRect.Center(), lockTexture.Width(), lockTexture.Height()),
				eg::ColorLin(1, 1, 1, 1)
			);
		}
		else if (level.status == LevelStatus::Completed)
		{
			glm::vec2 texSize = glm::vec2(completedTexture.Width(), completedTexture.Height()) * (1 + inflate);
			eg::Rectangle completeddTextureRect = eg::Rectangle(inflatedRect.Max() - texSize - 5.0f, texSize);
			m_spriteBatch.Draw(completedTexture, completeddTextureRect, eg::ColorLin(1, 1, 1, shade));
		}
	}
	m_spriteBatch.PopScissor();

	m_levelSelectScroll.contentHeight = totalHeight;
	m_levelSelectScroll.screenRectangle.x = boxLX + xOffset;
	m_levelSelectScroll.screenRectangle.y = boxEndY;
	m_levelSelectScroll.screenRectangle.w = boxW + 20;
	m_levelSelectScroll.screenRectangle.h = visibleHeight;
	m_levelSelectScroll.Update(m_currentFrameArgs, ComboBox::current == nullptr);
	m_levelSelectScroll.Draw(m_spriteBatch);

	m_levelSelectBackButton.position = glm::vec2(boxLX, boxEndY - 60);
	m_levelSelectBackButton.Update(m_currentFrameArgs, true);
	m_levelSelectBackButton.Draw(m_currentFrameArgs, m_spriteBatch);
}

float* menuWorldColorScale = eg::TweakVarFloat("menu_world_color_scale", 0.6f, 0.0f, 1.0f);

void MainMenuGameState::RenderWorld(float dt)
{
	if (m_world == nullptr)
	{
		if (backgroundLevel == nullptr || !IsLevelLoadingComplete(backgroundLevel->name))
			return;
		m_world = LoadLevelWorld(*backgroundLevel, false);
		m_physicsEngine = {};
		m_worldGameTime = 0;
		m_worldFadeInProgress = 0;
		GameRenderer::instance->WorldChanged(*m_world);
	}

	auto [renderResolutionX, renderResolutionY] = GameRenderer::GetScaledRenderResolution();

	eg::Format inputFormat = eg::Format::R8G8B8A8_UNorm;
	if (m_worldRenderTexture.handle == nullptr || renderResolutionX != m_worldRenderTexture.Width() ||
	    renderResolutionY != m_worldRenderTexture.Height() || inputFormat != m_worldRenderTexture.Format())
	{
		eg::TextureCreateInfo textureCI;
		textureCI.width = renderResolutionX;
		textureCI.height = renderResolutionY;
		textureCI.mipLevels = 1;
		textureCI.format = inputFormat;
		textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment;
		m_worldRenderTexture = eg::Texture::Create2D(textureCI);

		eg::FramebufferAttachment attachment(m_worldRenderTexture.handle);
		m_worldRenderFramebuffer = eg::Framebuffer({ &attachment, 1 });
	}
	m_worldBlurRenderer.MaybeUpdateResolution(renderResolutionX, renderResolutionY);

	m_worldGameTime += dt;

	GameRenderer::instance->m_player = nullptr;
	GameRenderer::instance->m_physicsDebugRenderer = nullptr;
	GameRenderer::instance->m_physicsEngine = &m_physicsEngine;
	GameRenderer::instance->m_particleManager = nullptr;
	GameRenderer::instance->m_gravityGun = nullptr;
	GameRenderer::instance->postColorScale = *menuWorldColorScale;
	GameRenderer::instance->SetViewMatrixFromThumbnailCamera(*m_world);

	WorldUpdateArgs updateArgs = {};
	updateArgs.mode = WorldMode::Menu;
	updateArgs.dt = dt;
	updateArgs.world = m_world.get();
	updateArgs.waterSim = GameRenderer::instance->m_waterSimulator.get();
	updateArgs.physicsEngine = &m_physicsEngine;
	updateArgs.plShadowMapper = &GameRenderer::instance->m_plShadowMapper;

	m_physicsEngine.BeginCollect();
	m_world->CollectPhysicsObjects(m_physicsEngine, dt);
	m_physicsEngine.EndCollect(dt);

	m_world->Update(updateArgs);

	{
		auto physicsUpdateCPUTimer = eg::StartCPUTimer("Physics");
		m_physicsEngine.Simulate(dt);
		m_physicsEngine.EndFrame(dt);
	}

	m_world->UpdateAfterPhysics(updateArgs);

	if (GameRenderer::instance->m_waterSimulator)
	{
		auto waterUpdateTimer = eg::StartCPUTimer("Water Update MT");
		GameRenderer::instance->m_waterSimulator->Update(eg::DC, dt, *m_world);
	}

	m_worldFadeInProgress = std::min(m_worldFadeInProgress + dt * 2, 1.0f);

	GameRenderer::instance->Render(
		*m_world, m_worldGameTime, dt, m_worldRenderFramebuffer.handle, inputFormat, renderResolutionX,
		renderResolutionY
	);

	m_worldRenderTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	m_worldBlurRenderer.Render(m_worldRenderTexture, settings.renderResolutionScale);

	eg::Rectangle dstRect;
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL)
	{
		dstRect = eg::Rectangle(
			0, m_currentFrameArgs.canvasHeight, m_currentFrameArgs.canvasWidth, -m_currentFrameArgs.canvasHeight
		);
	}
	else
	{
		dstRect = eg::Rectangle(0, 0, m_currentFrameArgs.canvasWidth, m_currentFrameArgs.canvasHeight);
	}

	const eg::Texture* blurredTexture = &m_worldBlurRenderer.OutputTexture();

	m_spriteBatch.Draw(
		*blurredTexture, dstRect, eg::ColorLin(1, 1, 1, m_worldFadeInProgress), eg::SpriteFlags::ForceLowestMipLevel
	);
}

void MainMenuGameState::OnDeactivate()
{
	GameRenderer::instance->m_waterSimulator = nullptr;
	m_physicsEngine = {};
	m_world = {};
}
