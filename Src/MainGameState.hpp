#pragma once

#include "GameState.hpp"
#include "Graphics/RenderContext.hpp"
#include "Graphics/Lighting/PointLightShadowMapper.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/PostProcessor.hpp"
#include "Graphics/ParticleRenderer.hpp"
#include "World/GravityGun.hpp"
#include "Graphics/WaterSimulator.hpp"
#include "Gui/PausedMenu.hpp"
#include "Graphics/SSR.hpp"
#include "Graphics/PhysicsDebugRenderer.hpp"
#include "Graphics/GlassBlurRenderer.hpp"

#include <EGame/Graphics/BloomRenderer.hpp>

class MainGameState : public GameState
{
public:
	explicit MainGameState(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
	void LoadWorld(std::istream& stream, int64_t levelIndex = -1, const class EntranceExitEnt* exitEntity = nullptr);
	
	void OnDeactivate() override;
	
private:
#ifndef NDEBUG
	void DrawOverlay(float dt);
#endif
	
	void RenderPointLightShadows(const PointLightShadowRenderArgs& args);
	
	bool ReloadLevel();
	
	int m_lastSettingsGeneration = -1;
	
	int64_t m_currentLevelIndex = -1;
	
	eg::PerspectiveProjection m_projection;
	RenderContext* m_renderCtx;
	WaterRenderer m_waterRenderer;
	SSR m_ssr;
	GlassBlurRenderer m_glassBlurRenderer;
	
	PointLightShadowMapper m_plShadowMapper;
	std::unique_ptr<eg::BloomRenderer> m_bloomRenderer;
	PostProcessor m_postProcessor;
	
	ParticleRenderer m_particleRenderer;
	eg::ParticleManager m_particleManager;
	
	WaterSimulator m_waterSimulator;
	std::shared_ptr<WaterSimulator::QueryAABB> m_playerWaterAABB;
	
	QualityLevel m_lightingQuality = QualityLevel::Low;
	
	std::unique_ptr<eg::BloomRenderer::RenderTarget> m_bloomRenderTarget;
	eg::Texture m_renderOutputTexture;
	
	float m_gameTime = 0;
	float m_delayedGameUpdateTime = 0;
	
	std::unique_ptr<World> m_world;
	Player m_player;
	GravityGun m_gravityGun;
	bool m_blurredTexturesNeeded = false;
	
	PausedMenu m_pausedMenu;
	
	PhysicsEngine m_physicsEngine;
	PhysicsDebugRenderer m_physicsDebugRenderer;
};

extern MainGameState* mainGameState;
