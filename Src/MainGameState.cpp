#include "MainGameState.hpp"
#include "Settings.hpp"
#include "Levels.hpp"
#include "World/Entities/EntTypes/EntranceExitEnt.hpp"
#include "MainMenuGameState.hpp"
#include "Gui/GuiCommon.hpp"

#include <fstream>
#include <imgui.h>

MainGameState* mainGameState;

static int* relativeMouseMode = eg::TweakVarInt("relms", 1, 0, 1);

MainGameState::MainGameState()
{
	eg::console::AddCommand("reload", 0, [this] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
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
	
	eg::console::AddCommand("curlevel", 0, [this] (eg::Span<const std::string_view> args, eg::console::Writer& writer)
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
	
	eg::console::AddCommand("showExtraLevels", 0, [this] (eg::Span<const std::string_view>, eg::console::Writer&)
	{
		settings.showExtraLevels = true;
	});
	
	const eg::Texture& particlesTexture = eg::GetAsset<eg::Texture>("Textures/Particles.png");
	m_particleManager.SetTextureSize(particlesTexture.Width(), particlesTexture.Height());
	
	m_playerWaterAABB = std::make_shared<WaterSimulator::QueryAABB>();
	GameRenderer::instance->m_waterSimulator.AddQueryAABB(m_playerWaterAABB);
	
	m_crosshairTexture = &eg::GetAsset<eg::Texture>("Textures/Crosshair.png");
}

void MainGameState::SetWorld(std::unique_ptr<World> newWorld, int64_t levelIndex, const EntranceExitEnt* exitEntity)
{
	m_player.Reset();
	m_gameTime = 0;
	m_currentLevelIndex = levelIndex;
	m_pausedMenu.isPaused = false;
	m_pausedMenu.shouldRestartLevel = false;
	
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
	
	if (levelIndex != -1)
	{
		mainMenuGameState->backgroundLevel = &levels[levelIndex];
	}
}

void MainGameState::OnDeactivate()
{
	GameRenderer::instance->m_waterSimulator.Stop();
	m_world.reset();
}

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
	
	m_pausedMenu.Update(dt);
	if (m_world == nullptr)
		return;
	
	if (!eg::console::IsShown() && !m_pausedMenu.isPaused)
	{
		auto worldUpdateCPUTimer = eg::StartCPUTimer("World Update");
		
		EntranceExitEnt* currentExit = nullptr;
		
		m_world->entManager.ForEachOfType<EntranceExitEnt>([&] (EntranceExitEnt& entity)
		{
			if (entity.ShouldSwitchEntrance())
				currentExit = &entity;
		});
		
		//Moves to the next level
		if (currentExit != nullptr && m_currentLevelIndex != -1 && levels[m_currentLevelIndex].nextLevelIndex != -1)
		{
			MarkLevelCompleted(levels[m_currentLevelIndex]);
			int64_t nextLevelIndex = levels[m_currentLevelIndex].nextLevelIndex;
			eg::Log(eg::LogLevel::Info, "lvl", "Going to next level '{0}'", levels[nextLevelIndex].name);
			SetWorld(LoadLevelWorld(levels[nextLevelIndex], false), nextLevelIndex, currentExit);
			m_physicsEngine = {};
		}
		
		WorldUpdateArgs updateArgs;
		updateArgs.mode = WorldMode::Game;
		updateArgs.dt = dt;
		updateArgs.player = &m_player;
		updateArgs.world = m_world.get();
		updateArgs.waterSim = &GameRenderer::instance->m_waterSimulator;
		updateArgs.physicsEngine = &m_physicsEngine;
		updateArgs.plShadowMapper = &GameRenderer::instance->m_plShadowMapper;
		
		eg::SetRelativeMouseMode(*relativeMouseMode);
		
		m_physicsEngine.BeginCollect();
		m_world->CollectPhysicsObjects(m_physicsEngine, dt);
		m_physicsEngine.EndCollect();
		
		{
			auto playerUpdateCPUTimer = eg::StartCPUTimer("Player Update");
			bool underwater = m_playerWaterAABB->GetResults().numIntersecting > *playerUnderwaterSpheres;
			m_player.Update(*m_world, m_physicsEngine, dt, underwater);
		}
		
		m_world->Update(updateArgs, &m_physicsEngine);
		
		UpdateViewProjMatrices();
		if (m_world->playerHasGravityGun)
		{
			auto gunUpdateCPUTimer = eg::StartCPUTimer("Gun Update");
			m_gravityGun.Update(*m_world, m_physicsEngine, GameRenderer::instance->m_waterSimulator, m_particleManager,
			                    m_player, GameRenderer::instance->GetInverseViewProjMatrix(), dt);
		}
	}
	else
	{
		eg::SetRelativeMouseMode(false);
		UpdateViewProjMatrices();
	}
	
	m_playerWaterAABB->SetAABB(m_player.GetAABB());
	
	{
		auto waterUpdateTimer = eg::StartCPUTimer("Water Update MT");
		GameRenderer::instance->m_waterSimulator.Update(*m_world, m_pausedMenu.isPaused);
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

static int* drawCrosshair = eg::TweakVarInt("draw_ch", 1, 0, 1);

void MainGameState::UpdateAndDrawHud(float dt)
{
	float controlHintTargetAlpha = 0;
	auto SetControlHint = [&] (std::string_view message, const KeyBinding& binding)
	{
		m_controlHintMessage = message;
		m_controlHintButton = IsControllerButton(eg::InputState::Current().PressedButton()) ? binding.controllerButton : binding.kbmButton;
		controlHintTargetAlpha = 1;
	};
	
	if (m_player.interactControlHint && m_world->showControlHint[(int)m_player.interactControlHint->type])
	{
		SetControlHint(m_player.interactControlHint->message, *m_player.interactControlHint->keyBinding);
	}
	else if (m_gravityGun.shouldShowControlHint)
	{
		SetControlHint("Change Gravitation", settings.keyShoot);
	}
	//For jump hint in intro_2:
	else if (m_currentLevelIndex != -1 && levels[m_currentLevelIndex].name == "intro_2" &&
	         m_player.OnGround() && m_player.CurrentDown() == Dir::NegX)
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
	
	constexpr float CONTROL_HINT_ANIMATION_TIME = 0.1f;
	m_controlHintAlpha = eg::AnimateTo(m_controlHintAlpha, controlHintTargetAlpha, dt / CONTROL_HINT_ANIMATION_TIME);
	
	//Draws the control hint
	if (m_controlHintAlpha > 0)
	{
		constexpr float FONT_SCALE = 1.0f;
		constexpr float BUTTON_BORDER_PADDING = 10;
		constexpr float BUTTON_MESSAGE_SPACING = 30;
		
		const std::string_view buttonName = eg::ButtonDisplayName(m_controlHintButton);
		const float buttonNameLen = style::UIFont->GetTextExtents(buttonName).x * FONT_SCALE;
		const float messageLen = style::UIFont->GetTextExtents(m_controlHintMessage).x * FONT_SCALE;
		const float buttonBoxSize = std::max(buttonNameLen, style::UIFont->LineHeight() * FONT_SCALE);
		
		const float totalLen = buttonBoxSize + BUTTON_MESSAGE_SPACING + messageLen;
		glm::vec2 leftPos((eg::CurrentResolutionX() - totalLen) / 2.0f, eg::CurrentResolutionY() * 0.15f);
		
		eg::ColorLin color(1, 1, 1, m_controlHintAlpha);
		
		eg::Rectangle buttonRect(
			leftPos.x + buttonNameLen / 2.0f - BUTTON_BORDER_PADDING - buttonBoxSize / 2.0f + 1,
			leftPos.y - BUTTON_BORDER_PADDING - 3,
			BUTTON_BORDER_PADDING * 2 + buttonBoxSize,
			BUTTON_BORDER_PADDING * 2 + style::UIFont->LineHeight() * FONT_SCALE
		);
		
		eg::SpriteBatch::overlay.DrawRect(buttonRect, eg::ColorLin(0, 0, 0, m_controlHintAlpha * 0.3f));
		eg::SpriteBatch::overlay.DrawRectBorder(buttonRect, color, 1);
		eg::SpriteBatch::overlay.DrawText(*style::UIFont, buttonName, leftPos, color, FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
		eg::SpriteBatch::overlay.DrawText(
			*style::UIFont, m_controlHintMessage, glm::vec2(leftPos.x + buttonBoxSize + BUTTON_MESSAGE_SPACING, leftPos.y),
			color, FONT_SCALE, nullptr, eg::TextFlags::DropShadow);
	}
	
	//Draws the crosshair
	if (*drawCrosshair)
	{
		const float crosshairSize = std::round(eg::CurrentResolutionX() / 40.0f);
		const eg::Rectangle chRect = eg::Rectangle::CreateCentered(
			eg::CurrentResolutionX() / 2,
			eg::CurrentResolutionY() / 2,
			crosshairSize, crosshairSize
		);
		
		constexpr float CROSSHAIR_OPACITY = 0.75f;
		const float opacity = CROSSHAIR_OPACITY * (1 - m_pausedMenu.fade);
		
		eg::SpriteBatch::overlay.Draw(*m_crosshairTexture, chRect, eg::ColorLin(1, 1, 1, opacity));
	}
}

int* debugOverlay = eg::TweakVarInt("dbg_overlay", 1);

void MainGameState::DrawOverlay(float dt)
{
	if (!*debugOverlay || !eg::DevMode())
		return;
	
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
	ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);
	
	const char* graphicsAPIName = "?";
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::OpenGL)
		graphicsAPIName = "OpenGL";
	else if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::Vulkan)
		graphicsAPIName = "Vulkan";
	
	ImGui::Text("FPS: %dHz | %.2fms", (int)(1.0f / dt), dt * 1000.0f);
	ImGui::Text("Graphics API: %s", graphicsAPIName);
	ImGui::Separator();
	m_player.DrawDebugOverlay();
	ImGui::Separator();
	ImGui::Text("Particles: %d", m_particleManager.ParticlesToDraw());
	ImGui::Text("PSM Last Update: %lld/%lld", GameRenderer::instance->m_plShadowMapper.LastUpdateFrameIndex(), eg::FrameIdx());
	ImGui::Text("PSM Updates: %lld", GameRenderer::instance->m_plShadowMapper.LastFrameUpdateCount());
	ImGui::Text("Water Spheres: %d", GameRenderer::instance->m_waterSimulator.NumParticles());
	ImGui::Text("Water Update Time: %.2fms", GameRenderer::instance->m_waterSimulator.LastUpdateTime() / 1E6);
	ImGui::Text("Level: %s", m_currentLevelIndex == -1 ? "?" : levels[m_currentLevelIndex].name.c_str());
	
	ImGui::End();
	ImGui::PopStyleVar();
}

bool MainGameState::ReloadLevel()
{
	if (m_currentLevelIndex == -1)
		return false;
	SetWorld(LoadLevelWorld(levels[m_currentLevelIndex], false), m_currentLevelIndex);
	return true;
}
