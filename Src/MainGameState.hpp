#pragma once

#include "GameState.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "World/GravityGun.hpp"
#include "Gui/PausedMenu.hpp"
#include "GameRenderer.hpp"
#include "Graphics/PhysicsDebugRenderer.hpp"

class MainGameState : public GameState
{
public:
	MainGameState();
	
	void RunFrame(float dt) override;
	
	void SetWorld(std::unique_ptr<World> newWorld,
		int64_t levelIndex = -1,
		const class EntranceExitEnt* exitEntity = nullptr,
		bool fromEditor = false);
	
	void OnDeactivate() override;
	
private:
	void DrawOverlay(float dt);
	
	void UpdateAndDrawHud(float dt);
	
	bool ReloadLevel();
	
	bool m_ssrReflectionColorEditorShown = false;
	
	const eg::Texture* m_crosshairTexture;
	
	float m_controlHintAlpha = 0;
	std::string_view m_controlHintMessage;
	eg::Button m_controlHintButton;
	
	int64_t m_currentLevelIndex = -1;
	
	float m_gameTime = 0;
	
	std::unique_ptr<World> m_world;
	Player m_player;
	GravityGun m_gravityGun;
	
	std::shared_ptr<WaterSimulator::QueryAABB> m_playerWaterAABB;
	
	PausedMenu m_pausedMenu;
	
	eg::ParticleManager m_particleManager;
	
	PhysicsEngine m_physicsEngine;
	PhysicsDebugRenderer m_physicsDebugRenderer;
};

extern MainGameState* mainGameState;
