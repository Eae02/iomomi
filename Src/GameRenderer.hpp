#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>
#include "Graphics/RenderContext.hpp"
#include "Graphics/GlassBlurRenderer.hpp"
#include "Graphics/SSR.hpp"
#include "Graphics/Lighting/PointLightShadowMapper.hpp"
#include "Graphics/PostProcessor.hpp"
#include "Graphics/ParticleRenderer.hpp"
#include "Graphics/WaterSimulator.hpp"
#include "Graphics/PhysicsDebugRenderer.hpp"

class GameRenderer
{
public:
	explicit GameRenderer(RenderContext& renderCtx);
	
	void WorldChanged(class World& world);
	
	void InvalidateShadows(const eg::Sphere& sphere)
	{
		m_plShadowMapper.Invalidate(sphere);
	}
	
	void SetViewMatrix(const glm::mat4& viewMatrix, const glm::mat4& viewMatrixInverse);
	
	const glm::mat4& GetViewMatrix() const { return m_viewMatrix; }
	const glm::mat4& GetInverseViewMatrix() const { return m_inverseViewMatrix; }
	const glm::mat4& GetViewProjMatrix() const { return m_viewProjMatrix; }
	const glm::mat4& GetInverseViewProjMatrix() const { return m_inverseViewProjMatrix; }
	
	void Render(World& world, float gameTime, float dt, eg::FramebufferHandle outputFramebuffer, uint32_t outputResX, uint32_t outputResY);
	
	class GravityGun* m_gravityGun = nullptr;
	const class Player* m_player = nullptr;
	eg::ParticleManager* m_particleManager = nullptr;
	class PhysicsEngine* m_physicsEngine = nullptr;
	class PhysicsDebugRenderer* m_physicsDebugRenderer = nullptr;
	
	WaterSimulator m_waterSimulator;
	
private:
	RenderTexManager m_rtManager;
	
	int m_lastSettingsGeneration = -1;
	
	eg::PerspectiveProjection m_projection;
	RenderContext* m_renderCtx;
	GlassBlurRenderer m_glassBlurRenderer;
	
	PointLightShadowMapper m_plShadowMapper;
	std::unique_ptr<eg::BloomRenderer> m_bloomRenderer;
	
	QualityLevel m_lightingQuality = QualityLevel::Low;
	
	std::unique_ptr<eg::BloomRenderer::RenderTarget> m_bloomRenderTarget;
	eg::Texture m_renderOutputTexture;
	
	bool m_blurredTexturesNeeded = false;
	
	glm::mat4 m_viewMatrix;
	glm::mat4 m_inverseViewMatrix;
	glm::mat4 m_viewProjMatrix;
	glm::mat4 m_inverseViewProjMatrix;
};