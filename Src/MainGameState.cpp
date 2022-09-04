#include "MainGameState.hpp"
#include "Settings.hpp"
#include "Levels.hpp"
#include "World/Entities/EntTypes/EntranceExitEnt.hpp"
#include "World/Entities/EntTypes/Activation/CubeEnt.hpp"
#include "MainMenuGameState.hpp"
#include "Gui/GuiCommon.hpp"
#include "AudioPlayers.hpp"

#include <iomanip>
#include <fstream>

#ifdef EG_HAS_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

MainGameState* mainGameState;

static int* relativeMouseMode = eg::TweakVarInt("relms", 1, 0, 1);

MainGameState::MainGameState()
{
	eg::console::AddCommand("reload", 0, [this] (std::span<const std::string_view> args, eg::console::Writer& writer)
	{
		if (!ReloadLevel())
		{
			writer.WriteLine(eg::console::ErrorColor, "No level to reload");
		}
		else
		{
			eg::console::Hide();
		}
	});
	
	eg::console::AddCommand("curlevel", 0, [this] (std::span<const std::string_view> args, eg::console::Writer& writer)
	{
		if (m_currentLevelIndex == -1)
		{
			writer.WriteLine(eg::console::ErrorColor, "No current level");
		}
		else
		{
			writer.WriteLine(eg::console::InfoColor, levels[m_currentLevelIndex].name);
		}
	});
	
	eg::console::AddCommand("ssrfbEdit", 0, [this] (std::span<const std::string_view> args, eg::console::Writer& writer)
	{
		m_ssrReflectionColorEditorShown = !m_ssrReflectionColorEditorShown;
	});
	
	eg::console::AddCommand("showExtraLevels", 0, [] (std::span<const std::string_view>, eg::console::Writer&)
	{
		settings.showExtraLevels = true;
	});
	
	eg::console::AddCommand("crosshair", 0, [] (std::span<const std::string_view>, eg::console::Writer&)
	{
		settings.drawCrosshair = !settings.drawCrosshair;
	});
	
	const eg::Texture& particlesTexture = eg::GetAsset<eg::Texture>("Textures/Particles.png");
	m_particleManager.SetTextureSize(particlesTexture.Width(), particlesTexture.Height());
	
	m_crosshairTexture = &eg::GetAsset<eg::Texture>("Textures/UI/Crosshair.png");
}

void MainGameState::SetWorld(std::unique_ptr<World> newWorld, int64_t levelIndex,
                             const EntranceExitEnt* exitEntity, bool fromEditor)
{
	m_player.Reset();
	m_gameTime = 0;
	m_currentLevelIndex = levelIndex;
	m_pausedMenu.isPaused = false;
	m_pausedMenu.shouldRestartLevel = false;
	m_pausedMenu.isFromEditor = fromEditor;
	m_ssrReflectionColorEditorShown = false;
	m_isPlayerCloseToExitWithWrongGravity = false;
	
	AudioPlayers::gameSFXPlayer.StopAll();
	
	//Moves the player to the entrance entity for this level
	newWorld->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
	{
		if (entity.m_type == EntranceExitEnt::Type::Entrance)
		{
			if (exitEntity != nullptr)
			{
				EntranceExitEnt::MovePlayer(*exitEntity, entity, m_player);
			}
			else
			{
				entity.InitPlayer(m_player);
			}
		}
	});
	
	m_world = std::move(newWorld);
	
	GameRenderer::instance->m_gravityGun = &m_gravityGun;
	GameRenderer::instance->WorldChanged(*m_world);
	
	if (GameRenderer::instance->m_waterSimulator)
		m_playerWaterAABB = GameRenderer::instance->m_waterSimulator->AddQueryAABB({});
	else
		m_playerWaterAABB = nullptr;
	
	if (levelIndex != -1)
	{
		mainMenuGameState->backgroundLevel = &levels[levelIndex];
	}
}

void MainGameState::OnDeactivate()
{
	GameRenderer::instance->m_waterSimulator = nullptr;
	AudioPlayers::gameSFXPlayer.StopAll();
	m_world.reset();
	m_relativeMouseModeLostListener.reset();
}

#ifdef IOMOMI_ENABLE_EDITOR
extern std::weak_ptr<World> runningEditorWorld;
#endif

static int* playerUnderwaterSpheres = eg::TweakVarInt("pl_underwater_spheres", 15, 0);

void MainGameState::RunFrame(float dt)
{
	GameRenderer::instance->m_player = &m_player;
	GameRenderer::instance->m_physicsDebugRenderer = &m_physicsDebugRenderer;
	GameRenderer::instance->m_physicsEngine = &m_physicsEngine;
	GameRenderer::instance->m_particleManager = &m_particleManager;
	GameRenderer::instance->m_gravityGun = &m_gravityGun;
	GameRenderer::instance->postColorScale = glm::mix(1.0f, 0.5f, m_pausedMenu.fade);
	
	constexpr float MAX_DT = 0.2f;
	if (dt > MAX_DT)
		dt = MAX_DT;
	
	bool frozenFrustum = eg::DevMode() && eg::IsButtonDown(eg::Button::F7);
	
	auto UpdateViewProjMatrices = [&] ()
	{
		glm::mat4 viewMatrix, inverseViewMatrix;
		m_player.GetViewMatrix(viewMatrix, inverseViewMatrix);
		GameRenderer::instance->SetViewMatrix(viewMatrix, inverseViewMatrix, !frozenFrustum);
	};
	
	if (!m_relativeMouseModeLostListener.has_value())
	{
		m_relativeMouseModeLostListener = eg::EventListener<eg::RelativeMouseModeLostEvent>();
	}
	m_relativeMouseModeLostListener->ProcessLast([&] (auto&) { m_pausedMenu.isPaused = true; });
	
	m_pausedMenu.Update(dt);
	if (m_world == nullptr)
		return;
	
	if (!eg::console::IsShown() && !m_pausedMenu.isPaused && !m_ssrReflectionColorEditorShown)
	{
		auto worldUpdateCPUTimer = eg::StartCPUTimer("World Update");
		
		EntranceExitEnt* currentExit = nullptr;
		
		m_isPlayerCloseToExitWithWrongGravity = false;
		m_world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
		{
			if (entity.IsPlayerCloseWithWrongGravity())
				m_isPlayerCloseToExitWithWrongGravity = true;
			if (entity.ShouldSwitchEntrance())
				currentExit = &entity;
		});
		
		//Moves to the next level
		if (currentExit != nullptr && m_currentLevelIndex != -1 && levels[m_currentLevelIndex].nextLevelIndex != -1)
		{
			glm::quat oldPlayerRotation = m_player.Rotation();
			MarkLevelCompleted(levels[m_currentLevelIndex]);
			int64_t nextLevelIndex = levels[m_currentLevelIndex].nextLevelIndex;
			SetWorld(LoadLevelWorld(levels[nextLevelIndex], false), nextLevelIndex, currentExit);
			m_gravityGun.ChangeLevel(oldPlayerRotation, m_player.Rotation());
			m_physicsEngine = {};
			UpdateViewProjMatrices();
		}
		
		WorldUpdateArgs updateArgs;
		updateArgs.mode = WorldMode::Game;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.waterSim = GameRenderer::instance->m_waterSimulator.get();
		updateArgs.physicsEngine = &m_physicsEngine;
		updateArgs.plShadowMapper = &GameRenderer::instance->m_plShadowMapper;
		
		eg::SetRelativeMouseMode(*relativeMouseMode);
		
		m_physicsEngine.BeginCollect();
		m_world->CollectPhysicsObjects(m_physicsEngine, dt);
		m_physicsEngine.EndCollect(dt);
		
		m_world->Update(updateArgs);
		
		{
			auto physicsUpdateCPUTimer = eg::StartCPUTimer("Physics (early)");
			m_physicsEngine.Simulate(dt);
		}
		
		{
			auto playerUpdateCPUTimer = eg::StartCPUTimer("Player Update");
			bool underwater = false;
			if (m_playerWaterAABB)
				underwater = m_playerWaterAABB->GetResults().numIntersecting > *playerUnderwaterSpheres;
			m_player.Update(*m_world, m_physicsEngine, dt, underwater);
		}
		
		m_world->entManager.ForEachOfType<CubeEnt>([&] (CubeEnt& cube) { cube.UpdateAfterPlayer(updateArgs); });
		
		{
			auto physicsUpdateCPUTimer = eg::StartCPUTimer("Physics (late)");
			m_physicsEngine.Simulate(dt);
			m_physicsEngine.EndFrame(dt);
		}
		
		m_world->UpdateAfterPhysics(updateArgs);
		
		eg::AudioLocationParameters playerALP;
		playerALP.position = m_player.Position();
		playerALP.direction = m_player.Forward();
		playerALP.velocity = m_player.Velocity();
		eg::UpdateAudioListener(playerALP, -DirectionVector(m_player.CurrentDown()));
		
		UpdateViewProjMatrices();
		if (m_world->playerHasGravityGun)
		{
			auto gunUpdateCPUTimer = eg::StartCPUTimer("Gun Update");
			m_gravityGun.Update(*m_world, m_physicsEngine, GameRenderer::instance->m_waterSimulator.get(),
			                    m_particleManager, m_player, GameRenderer::instance->GetInverseViewProjMatrix(), dt);
		}
	}
	else
	{
		eg::SetRelativeMouseMode(false);
		UpdateViewProjMatrices();
	}
	
#ifdef EG_HAS_IMGUI
	if (m_ssrReflectionColorEditorShown)
	{
		ImGui::Begin("SSR Reflection Color Editor", &m_ssrReflectionColorEditorShown);
		ImGui::ColorPicker3("###Color", &m_world->ssrFallbackColor.r);
		ImGui::DragFloat("Intensity", &m_world->ssrIntensity, 0.1f);
		GameRenderer::instance->UpdateSSRParameters(*m_world);
		
		std::shared_ptr<World> editorWorld = runningEditorWorld.lock();
		const bool canSaveToEditor = m_pausedMenu.isFromEditor && editorWorld != nullptr;
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !canSaveToEditor);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, canSaveToEditor ? 1.0f : 0.4f);
		if (ImGui::Button("Save to Editor") && canSaveToEditor)
		{
			editorWorld->ssrFallbackColor = m_world->ssrFallbackColor;
			editorWorld->ssrIntensity = m_world->ssrIntensity;
		}
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
		
		ImGui::End();
	}
#endif
	
	if (m_playerWaterAABB)
	{
		m_playerWaterAABB->SetAABB(m_player.GetAABB());
	}
	
	if (GameRenderer::instance->m_waterSimulator)
	{
		auto waterUpdateTimer = eg::StartCPUTimer("Water Update MT");
		GameRenderer::instance->m_waterSimulator->Update(*m_world, m_player.EyePosition(), m_pausedMenu.isPaused);
	}
	
	GameRenderer::instance->Render(*m_world, m_gameTime, dt, nullptr, eg::CurrentResolutionX(), eg::CurrentResolutionY());
	
	UpdateAndDrawHud(dt);
	
	m_pausedMenu.Draw(eg::SpriteBatch::overlay);
	if (m_pausedMenu.shouldRestartLevel)
	{
		ReloadLevel();
	}
	
	DrawOverlay(dt);
	
	if (!m_pausedMenu.isPaused)
	{
		m_gameTime += dt;
	}
}

constexpr float CONTROL_HINT_ANIMATION_TIME = 0.1f;

float* errorHintFontScale = eg::TweakVarFloat("ehint_font_scale", 0.8f, 0.0f);
float* controlHintFontScale = eg::TweakVarFloat("chint_font_scale", 1.0f, 0.0f);

void MainGameState::UpdateAndDrawHud(float dt)
{
	float controlHintTargetAlpha = 0;
	auto SetControlHint = [&] (std::string_view message, const KeyBinding& binding)
	{
		m_controlHintMessage = message;
		m_controlHintButton = IsControllerButton(eg::InputState::Current().PressedButton())
			? binding.controllerButton : binding.kbmButton;
		controlHintTargetAlpha = 1;
	};
	
	const std::string_view currentLevelName = m_currentLevelIndex != -1 ? std::string_view(levels[m_currentLevelIndex].name) : "";
	
	if (m_player.interactControlHint)
	{
		auto optControlHint = m_player.interactControlHint->optControlHintType;
		if (!optControlHint.has_value() || m_world->showControlHint[(int)*optControlHint])
		{
			SetControlHint(m_player.interactControlHint->message, *m_player.interactControlHint->keyBinding);
		}
	}
	else if (m_gravityGun.shouldShowControlHint)
	{
		SetControlHint("Change Gravitation", settings.keyShoot);
	}
	//For jump hint in intro_2:
	else if (currentLevelName == "intro_2" && m_player.OnGround() && m_player.CurrentDown() == Dir::NegX)
	{
		const glm::vec3 controlHintMin(8, 5, -9);
		const glm::vec3 controlHintMax(11, 7, -4);
		if (controlHintMin.x <= m_player.Position().x && m_player.Position().x <= controlHintMax.x &&
			controlHintMin.y <= m_player.Position().y && m_player.Position().y <= controlHintMax.y &&
			controlHintMin.z <= m_player.Position().z && m_player.Position().z <= controlHintMax.z)
		{
			SetControlHint("Jump", settings.keyJump);
		}
	}
	
	float errorHintTargetAlpha = 0;
	if (m_world->showControlHint[static_cast<int>(OptionalControlHintType::CannotChangeGravityWhenCarrying)] &&
		m_player.IsCarryingAndTouchingGravityCorner())
	{
		m_errorHintMessage = "It is not possible to change gravity while carrying a cube";
		errorHintTargetAlpha = 1;
	}
	else if (m_world->showControlHint[static_cast<int>(OptionalControlHintType::CannotExitWithWrongGravity)] &&
		m_isPlayerCloseToExitWithWrongGravity)
	{
		m_errorHintMessage = "Your gravity must point down in order to exit";
		errorHintTargetAlpha = 1;
	}
	
	m_controlHintAlpha = eg::AnimateTo(m_controlHintAlpha, controlHintTargetAlpha, dt / CONTROL_HINT_ANIMATION_TIME);
	m_errorHintAlpha = eg::AnimateTo(m_errorHintAlpha, errorHintTargetAlpha, dt / CONTROL_HINT_ANIMATION_TIME);
	
	//Draws the control hint
	if (m_controlHintAlpha > 0)
	{
		const float controlHintY = eg::CurrentResolutionY() * 0.15f;
		
		constexpr float BUTTON_BORDER_PADDING = 10;
		constexpr float BUTTON_MESSAGE_SPACING = 30;
		
		const std::string_view buttonName = eg::ButtonDisplayName(m_controlHintButton);
		const float buttonNameLen = style::UIFont->GetTextExtents(buttonName).x * *controlHintFontScale;
		const float messageLen = style::UIFont->GetTextExtents(m_controlHintMessage).x * *controlHintFontScale;
		const float buttonBoxSize = std::max(buttonNameLen, style::UIFont->LineHeight() * *controlHintFontScale);
		
		const float totalLen = buttonBoxSize + BUTTON_MESSAGE_SPACING + messageLen;
		glm::vec2 leftPos((eg::CurrentResolutionX() - totalLen) / 2.0f, controlHintY);
		
		eg::ColorLin color(1, 1, 1, m_controlHintAlpha);
		
		eg::Rectangle buttonRect(
			leftPos.x + buttonNameLen / 2.0f - BUTTON_BORDER_PADDING - buttonBoxSize / 2.0f + 1,
			leftPos.y - BUTTON_BORDER_PADDING - 3,
			BUTTON_BORDER_PADDING * 2 + buttonBoxSize,
			BUTTON_BORDER_PADDING * 2 + style::UIFont->LineHeight() * *controlHintFontScale
		);
		
		eg::SpriteBatch::overlay.DrawRect(buttonRect, eg::ColorLin(0, 0, 0, m_controlHintAlpha * 0.3f));
		eg::SpriteBatch::overlay.DrawRectBorder(buttonRect, color, 1);
		eg::SpriteBatch::overlay.DrawText(
			*style::UIFont, buttonName, leftPos, color, *controlHintFontScale, nullptr, eg::TextFlags::DropShadow);
		eg::SpriteBatch::overlay.DrawText(
			*style::UIFont, m_controlHintMessage, glm::vec2(leftPos.x + buttonBoxSize + BUTTON_MESSAGE_SPACING, leftPos.y),
			color, *controlHintFontScale, nullptr, eg::TextFlags::DropShadow);
	}
	
	//Draws the error hint
	if (m_errorHintAlpha > 0)
	{
		const float errorHintY = eg::CurrentResolutionY() * 0.9f - style::UIFont->LineHeight() * *errorHintFontScale;
		
		const eg::ColorLin color = eg::ColorLin(eg::ColorSRGB::FromHex(0xffb3c1)).ScaleAlpha(m_errorHintAlpha);
		
		const float messageLen = style::UIFont->GetTextExtents(m_errorHintMessage).x * *errorHintFontScale;
		const float leftX = (eg::CurrentResolutionX() - messageLen) / 2.0f;
		
		constexpr float RECT_PADDING = 10;
		
		eg::SpriteBatch::overlay.DrawRect(
			eg::Rectangle(
				leftX - RECT_PADDING,
				errorHintY - RECT_PADDING,
				messageLen + RECT_PADDING * 2,
				style::UIFont->LineHeight() * *errorHintFontScale + RECT_PADDING * 2),
			eg::ColorLin(0, 0, 0, 0.3f));
		
		eg::SpriteBatch::overlay.DrawText(
			*style::UIFont, m_errorHintMessage, glm::vec2(leftX, errorHintY),
			color, *errorHintFontScale, nullptr, eg::TextFlags::DropShadow);
	}
	
	//Draws the crosshair
	if (settings.drawCrosshair)
	{
		const float crosshairSize = std::round(eg::CurrentResolutionX() / 40.0f);
		const eg::Rectangle chRect = eg::Rectangle::CreateCentered(
			static_cast<float>(eg::CurrentResolutionX()) / 2.0f,
			static_cast<float>(eg::CurrentResolutionY()) / 2.0f,
			crosshairSize, crosshairSize
		);
		
		constexpr float CROSSHAIR_OPACITY = 0.75f;
		const float opacity = CROSSHAIR_OPACITY * (1 - m_pausedMenu.fade);
		
		eg::SpriteBatch::overlay.Draw(*m_crosshairTexture, chRect, eg::ColorLin(1, 1, 1, opacity));
	}
}

int* debugOverlay = eg::TweakVarInt("dbg_overlay", 0, 0, 1);

void MainGameState::DrawOverlay(float dt)
{
#ifdef NDEBUG
	if (!*debugOverlay) return;
#else
	if (*debugOverlay) return;
#endif
	
	std::ostringstream textStream;
	textStream << std::setprecision(2) << std::fixed;
	
	textStream << "FPS: " << static_cast<int>(1.0f / dt) << "Hz | " << (dt * 1000.0f) << "ms\n";
	
	textStream << "GfxAPI: ";
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL)
		textStream << "OpenGL";
	else if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan)
		textStream << "Vulkan";
	else
		textStream << "?";
	textStream << "\n";
	
	textStream << "GfxDevice: " << eg::GetGraphicsDeviceInfo().deviceName << "\n";
	
	if (eg::gal::GetMemoryStat)
	{
		eg::GraphicsMemoryStat memoryStat = eg::gal::GetMemoryStat();
		textStream << "GfxMemUsage: " <<
			eg::ReadableBytesSize(memoryStat.allocatedBytesGPU) << " " <<
			eg::ReadableBytesSize(memoryStat.allocatedBytes) << "\n";
	}
	
	textStream << "-\n";
	
	m_player.GetDebugText(textStream);
	
	textStream << "-\n";
	textStream << "Particles: " << m_particleManager.ParticlesToDraw() << "\n";
	
	textStream << "PSM Last Update: " <<
		GameRenderer::instance->m_plShadowMapper.LastUpdateFrameIndex() << "/" << eg::FrameIdx() << "\n";
	
	textStream << "PSM Updates: " << GameRenderer::instance->m_plShadowMapper.LastFrameUpdateCount() << "\n";
	
	if (const IWaterSimulator* waterSim = GameRenderer::instance->m_waterSimulator.get())
	{
		textStream << "Water Spheres: " << waterSim->NumParticles() << "\n";
		textStream << "Water Update Time: " << (static_cast<double>(waterSim->LastUpdateTime()) / 1E6) << "ms\n";
	}
	
	if (m_currentLevelIndex != -1)
	{
		textStream << "Level: " << levels[m_currentLevelIndex].name << "\n";
	}
	
	std::string text = textStream.str();
	std::vector<std::string_view> lines;
	eg::SplitString(text, '\n', lines);
	
	const float lineHeight = eg::SpriteFont::DevFont().LineHeight();
	const float padding = 5;
	const float separatorSpacing = 3;
	const eg::ColorLin textColor(1, 1, 1, 0.75f);
	const eg::ColorLin loTextColor(1, 0.8f, 0.8f, 0.5f);
	
	float y = static_cast<float>(eg::CurrentResolutionY()) - padding;
	for (std::string_view line : lines)
	{
		if (line == "-")
		{
			float lineY = y - separatorSpacing / 2;
			eg::SpriteBatch::overlay.DrawLine(glm::vec2(padding, lineY), glm::vec2(100 - padding, lineY), textColor, 0.5f);
			y -= separatorSpacing;
		}
		else
		{
			y -= lineHeight;
			eg::SpriteBatch::overlay.DrawText(eg::SpriteFont::DevFont(), line, glm::vec2(padding, y),
			                                  textColor, 1, nullptr, eg::TextFlags::DropShadow, &loTextColor);
		}
	}
}

bool MainGameState::ReloadLevel()
{
	if (m_currentLevelIndex == -1)
		return false;
	SetWorld(LoadLevelWorld(levels[m_currentLevelIndex], false), m_currentLevelIndex);
	return true;
}
