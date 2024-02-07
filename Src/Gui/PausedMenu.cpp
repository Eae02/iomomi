#include "PausedMenu.hpp"

#include "../GameState.hpp"
#include "../MainMenuGameState.hpp"
#include "../Settings.hpp"
#include "OptionsMenu.hpp"

#ifdef IOMOMI_ENABLE_EDITOR
#include "../Editor/Editor.hpp"
#endif

constexpr float BUTTON_W = 200;

PausedMenu::PausedMenu() : m_widgetList(BUTTON_W)
{
	m_widgetList.AddWidget(Button(
		"Resume",
		[&]
		{
			isPaused = false;
			optionsMenuOpen = false;
		}));
	m_widgetList.AddWidget(Button(
		"Restart Level",
		[&]
		{
			shouldRestartLevel = true;
			isPaused = false;
			optionsMenuOpen = false;
		}));
	m_widgetList.AddWidget(Button("Options", [&] { optionsMenuOpen = true; }));
	m_mainMenuWidgetIndex = m_widgetList.AddWidget(Button(
		"",
		[&]
		{
#ifdef IOMOMI_ENABLE_EDITOR
			if (isFromEditor)
			{
				SetCurrentGS(editor);
				return;
			}
#endif
			mainMenuGameState->GoToMainScreen();
			SetCurrentGS(mainMenuGameState);
		}));

	m_widgetList.relativeOffset = glm::vec2(-0.5f, 0.5f);
}

constexpr float FADE_IN_TIME = 0.2f;

void PausedMenu::Update(float dt)
{
	m_currentFrameArgs = GuiFrameArgs::MakeForCurrentFrame(dt);

	shouldRestartLevel = false;

	if (settings.keyMenu.IsDown() && !settings.keyMenu.WasDown() && !KeyBindingWidget::anyKeyBindingPickingKey)
	{
		isPaused = !isPaused;
		optionsMenuOpen = false;
	}

	float fadeTarget = 0;
	float optionsFadeTarget = 0;

	if (isPaused)
	{
		fadeTarget = 1;
		optionsFadeTarget = optionsMenuOpen ? 1 : 0;

		if (m_optionsMenuOverlayFade > 0)
		{
			UpdateOptionsMenu(m_currentFrameArgs, glm::vec2(0), optionsMenuOpen);
		}

		std::get<Button>(m_widgetList.GetWidget(m_mainMenuWidgetIndex)).text =
			isFromEditor ? "Return to Editor" : "Main Menu";
		m_widgetList.position = glm::vec2(m_currentFrameArgs.canvasWidth, m_currentFrameArgs.canvasHeight) / 2.0f;
		m_widgetList.Update(m_currentFrameArgs, !optionsMenuOpen);
	}

	fade = eg::AnimateTo(fade, fadeTarget, dt / FADE_IN_TIME);
	m_optionsMenuOverlayFade = eg::AnimateTo(m_optionsMenuOverlayFade, optionsFadeTarget, dt / FADE_IN_TIME);
}

void PausedMenu::Draw()
{
	if (!isPaused)
		return;

	m_spriteBatch.Reset();

	if (m_optionsMenuOverlayFade > 0)
	{
		m_spriteBatch.DrawRect(
			eg::Rectangle(0, 0, m_currentFrameArgs.canvasWidth, m_currentFrameArgs.canvasHeight),
			eg::ColorLin(0, 0, 0, 0.3f * m_optionsMenuOverlayFade));
	}

	if (optionsMenuOpen)
	{
		DrawOptionsMenu(m_currentFrameArgs, m_spriteBatch);
	}
	else
	{
		m_widgetList.Draw(m_currentFrameArgs, m_spriteBatch);
	}

	if (ComboBox::current)
	{
		ComboBox::current->DrawOverlay(m_currentFrameArgs, m_spriteBatch);
	}

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	m_spriteBatch.UploadAndRender(
		eg::SpriteBatch::RenderArgs{
			.framebufferFormat = eg::ColorAndDepthFormat(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil),
			.matrix = m_currentFrameArgs.GetMatrixToNDC(),
		},
		rpBeginInfo);
}
