#pragma once

#include "GameState.hpp"
#include "Graphics/RenderContext.hpp"
#include "Graphics/Lighting/PointLightShadowMapper.hpp"
#include "Graphics/Lighting/LightProbesManager.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/PostProcessor.hpp"
#include "Graphics/PlanarReflectionsManager.hpp"
#include "Graphics/ParticleRenderer.hpp"
#include "World/GravityGun.hpp"
#include "Graphics/WaterSimulator.hpp"

#include <EGame/Graphics/BloomRenderer.hpp>

class MainGameState : public GameState
{
public:
	explicit MainGameState(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
	void LoadWorld(std::istream& stream, int64_t levelIndex = -1, const eg::Entity* exitEntity = nullptr);
	
private:
	void DoDeferredRendering(bool useLightProbes, DeferredRenderer::RenderTarget& renderTarget);
	
	void RenderPlanarReflections(const ReflectionPlane& plane, eg::FramebufferRef framebuffer);
	
	void DrawOverlay(float dt);
	
	void RenderPointLightShadows(const PointLightShadowRenderArgs& args);
	
	int m_lastSettingsGeneration = -1;
	
	int64_t m_currentLevelIndex = -1;
	
	eg::PerspectiveProjection m_projection;
	RenderContext* m_renderCtx;
	
	PrepareDrawArgs m_prepareDrawArgs;
	PointLightShadowMapper m_plShadowMapper;
	std::unique_ptr<eg::BloomRenderer> m_bloomRenderer;
	PostProcessor m_postProcessor;
	PlanarReflectionsManager m_planarReflectionsManager;
	
	ParticleRenderer m_particleRenderer;
	eg::ParticleManager m_particleManager;
	
	WaterSimulator m_waterSimulator;
	
	QualityLevel m_lightingQuality = QualityLevel::Low;
	uint32_t m_msaaSamples = 0;
	
	std::unique_ptr<DeferredRenderer::RenderTarget> m_renderTarget;
	std::unique_ptr<eg::BloomRenderer::RenderTarget> m_bloomRenderTarget;
	eg::Texture m_renderOutputTexture;
	
	float m_gameTime = 0;
	bool m_relativeMouseMode = !eg::DevMode();
	
	std::unique_ptr<World> m_world;
	Player m_player;
	GravityGun m_gravityGun;
};

extern MainGameState* mainGameState;
