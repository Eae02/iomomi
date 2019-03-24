#pragma once

#include "GameState.hpp"
#include "Graphics/RenderContext.hpp"
#include "Graphics/Lighting/PointLightShadowMapper.hpp"
#include "Graphics/Lighting/LightProbesManager.hpp"
#include "World/World.hpp"
#include "World/Player.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "Graphics/PostProcessor.hpp"

class MainGameState : public GameState
{
public:
	explicit MainGameState(RenderContext& renderCtx);
	
	void RunFrame(float dt) override;
	
	void SetResolution(int width, int height);
	
	void LoadWorld(std::istream& stream);
	
private:
	void DoDeferredRendering(bool useLightProbes, DeferredRenderer::RenderTarget& renderTarget);
	
	void DrawOverlay(float dt);
	
	void RenderPointLightShadows(const PointLightShadowRenderArgs& args);
	
	eg::PerspectiveProjection m_projection;
	RenderContext* m_renderCtx;
	
	PrepareDrawArgs m_prepareDrawArgs;
	PointLightShadowMapper m_plShadowMapper;
	PostProcessor m_postProcessor;
	LightProbesManager m_lightProbesManager;
	
	std::unique_ptr<DeferredRenderer::RenderTarget> m_renderTarget;
	eg::Texture m_renderOutputTexture;
	
	float m_gameTime = 0;
	
	World m_world;
	Player m_player;
};

extern MainGameState* mainGameState;
