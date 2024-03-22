#pragma once

#include <EGame/Graphics/BloomRenderer.hpp>

#include "Graphics/BlurRenderer.hpp"
#include "Graphics/DeferredRenderer.hpp"
#include "Graphics/Lighting/PointLightShadowMapper.hpp"
#include "Graphics/ParticleRenderer.hpp"
#include "Graphics/PhysicsDebugRenderer.hpp"
#include "Graphics/PostProcessor.hpp"
#include "Graphics/RenderTargets.hpp"
#include "Graphics/SSAO.hpp"
#include "Graphics/SSR.hpp"
#include "Settings.hpp"
#include "Water/WaterBarrierRenderer.hpp"
#include "Water/WaterRenderer.hpp"
#include "Water/WaterSimulator.hpp"

class GameRenderer
{
public:
	GameRenderer();

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
		uint32_t outputResX, uint32_t outputResY
	);

	static GameRenderer* instance;

	class GravityGun* m_gravityGun = nullptr;
	const class Player* m_player = nullptr;
	eg::ParticleManager* m_particleManager = nullptr;
	class PhysicsEngine* m_physicsEngine = nullptr;
	class PhysicsDebugRenderer* m_physicsDebugRenderer = nullptr;

	std::unique_ptr<WaterSimulator2> m_waterSimulator;
	PointLightShadowMapper m_plShadowMapper;
	float postColorScale = 1;
	std::optional<float> fovOverride;

	void InitWaterSimulator(World& world);

	static std::pair<uint32_t, uint32_t> GetScaledRenderResolution();

private:
	RenderTargets m_renderTargets;

	int m_lastSettingsGeneration = -1;

	DeferredRenderer m_deferredRenderer;
	ParticleRenderer m_particleRenderer;

	PostProcessor m_postProcessor;

	eg::MeshBatch m_meshBatch;
	eg::MeshBatchOrdered m_transparentMeshBatch;

	eg::PerspectiveProjection m_projection;
	BlurRenderer m_glassBlurRenderer;

	std::optional<SSAO> m_ssao;

	std::optional<SSR> m_ssr;
	eg::ColorLin m_ssrFallbackColor = eg::ColorLin(eg::ColorLin::White);
	float m_ssrIntensity = 1.0f;

	std::optional<WaterRenderer> m_waterRenderer;

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
