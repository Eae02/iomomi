#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>

#include "Graphics/BlurRenderer.hpp"
#include "Graphics/Lighting/PointLightShadowMapper.hpp"
#include "Graphics/ParticleRenderer.hpp"
#include "Graphics/PhysicsDebugRenderer.hpp"
#include "Graphics/PostProcessor.hpp"
#include "Graphics/RenderContext.hpp"
#include "Graphics/SSR.hpp"
#include "Graphics/Water/WaterBarrierRenderer.hpp"
#include "Settings.hpp"

class GameRenderer
{
public:
	explicit GameRenderer(RenderContext& renderCtx);

	void WorldChanged(class World& world);

	void UpdateSSRParameters(const class World& world);

	void SetViewMatrix(const glm::mat4& viewMatrix, const glm::mat4& viewMatrixInverse, bool updateFrustum = true);
	void SetViewMatrixFromThumbnailCamera(const class World& world);

	const glm::mat4& GetViewMatrix() const { return m_viewMatrix; }
	const glm::mat4& GetInverseViewMatrix() const { return m_inverseViewMatrix; }
	const glm::mat4& GetViewProjMatrix() const { return m_viewProjMatrix; }
	const glm::mat4& GetInverseViewProjMatrix() const { return m_inverseViewProjMatrix; }

	void Render(
		World& world, float gameTime, float dt, eg::FramebufferHandle outputFramebuffer, eg::Format outputFormat,
		uint32_t outputResX, uint32_t outputResY);

	static GameRenderer* instance;

	class GravityGun* m_gravityGun = nullptr;
	const class Player* m_player = nullptr;
	eg::ParticleManager* m_particleManager = nullptr;
	class PhysicsEngine* m_physicsEngine = nullptr;
	class PhysicsDebugRenderer* m_physicsDebugRenderer = nullptr;

	std::unique_ptr<IWaterSimulator> m_waterSimulator;
	PointLightShadowMapper m_plShadowMapper;
	float postColorScale = 1;

private:
	RenderTexManager m_rtManager;

	int m_lastSettingsGeneration = -1;

	eg::PerspectiveProjection m_projection;
	RenderContext* m_renderCtx;
	BlurRenderer m_glassBlurRenderer;

	SSR::SSRRenderArgs m_ssrRenderArgs;

	WaterBarrierRenderer m_waterBarrierRenderer;

	std::unique_ptr<eg::BloomRenderer> m_bloomRenderer;

	QualityLevel m_lightingQuality = QualityLevel::Low;

	BloomQuality m_oldBloomQuality = BloomQuality::Off;
	std::unique_ptr<eg::BloomRenderer::RenderTarget> m_bloomRenderTarget;
	eg::Texture m_renderOutputTexture;

	bool m_blurredTexturesNeeded = false;

	glm::mat4 m_viewMatrix;
	glm::mat4 m_inverseViewMatrix;
	glm::mat4 m_viewProjMatrix;
	glm::mat4 m_inverseViewProjMatrix;

	std::vector<std::shared_ptr<PointLight>> m_pointLights;
	std::vector<eg::MeshBatch> m_pointLightMeshBatches;

	eg::Frustum m_frustum;
};
