#include "GameRenderer.hpp"

#include "Graphics/GraphicsCommon.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"
#include "Graphics/Materials/GravitySwitchVolLightMaterial.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/RenderSettings.hpp"
#include "Settings.hpp"
#include "Water/WaterGenerate.hpp"
#include "Water/WaterSimulationShaders.hpp"
#include "Water/WaterSimulator.hpp"
#include "World/Entities/EntTypes/Visual/WindowEnt.hpp"
#include "World/GravityGun.hpp"
#include "World/Player.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "World/World.hpp"

GameRenderer* GameRenderer::instance;

GameRenderer::GameRenderer() : m_glassBlurRenderer(3, eg::Format::R16G16B16A16_Float)
{
	m_projection.SetZNear(ZNear);
	m_projection.SetZFar(ZFar);
}

void GameRenderer::InitWaterSimulator(World& world)
{
	if (!waterSimShaders.isWaterSupported)
		return;

	std::vector<glm::vec3> waterPositions = GenerateWaterPositions(world);
	if (waterPositions.empty())
	{
		m_waterSimulator = nullptr;
		m_waterRenderer = std::nullopt;
	}
	else
	{
		auto waterCollisionQuads = GenerateWaterCollisionQuads(world);

		m_waterSimulator = std::make_unique<WaterSimulator2>(
			WaterSimulatorInitArgs{
				.positions = waterPositions,
				.voxelBuffer = &world.voxels,
				.collisionQuads = std::move(waterCollisionQuads),
			},
			eg::DC
		);

		if (!m_waterRenderer.has_value())
		{
			m_waterRenderer = WaterRenderer();

			if (m_renderTargets.waterPostStage.has_value())
			{
				m_waterRenderer->RenderTargetsCreated(m_renderTargets);
			}
		}
		m_waterRenderer->SetPositionsBuffer(
			m_waterSimulator->GetPositionsGPUBuffer(), m_waterSimulator->NumParticles()
		);
	}

	m_waterBarrierRenderer.Init(m_waterSimulator.get(), world);
}

void GameRenderer::WorldChanged(World& world)
{
	InitWaterSimulator(world);

	m_pointLights.clear();
	world.entManager.ForEach([&](Ent& entity) { entity.CollectPointLights(m_pointLights); });
	if (world.playerHasGravityGun && m_gravityGun)
	{
		m_pointLights.push_back(m_gravityGun->light);
	}

	m_plShadowMapper.SetLightSources(m_pointLights);

	m_blurredTexturesNeeded = false;
	world.entManager.ForEachOfType<WindowEnt>(
		[&](const WindowEnt& windowEnt)
		{
			if (windowEnt.NeedsBlurredTextures())
				m_blurredTexturesNeeded = true;
		}
	);

	UpdateSSRParameters(world);
}

void GameRenderer::UpdateSSRParameters(const World& world)
{
	m_ssrFallbackColor = eg::ColorLin(world.ssrFallbackColor);
	m_ssrIntensity = world.ssrIntensity;
}

void GameRenderer::SetViewMatrix(const glm::mat4& viewMatrix, const glm::mat4& inverseViewMatrix, bool updateFrustum)
{
	m_viewMatrix = viewMatrix;
	m_inverseViewMatrix = inverseViewMatrix;
	m_viewProjMatrix = m_projection.Matrix() * viewMatrix;
	m_inverseViewProjMatrix = inverseViewMatrix * m_projection.InverseMatrix();
	if (updateFrustum)
	{
		m_frustum = eg::Frustum(m_inverseViewProjMatrix);
	}
}

void GameRenderer::SetViewMatrixFromThumbnailCamera(const World& world)
{
	m_viewMatrix =
		glm::lookAt(world.thumbnailCameraPos, world.thumbnailCameraPos + world.thumbnailCameraDir, glm::vec3(0, 1, 0));
	m_inverseViewMatrix = glm::inverse(m_viewMatrix);
	m_viewProjMatrix = m_projection.Matrix() * m_viewMatrix;
	m_inverseViewProjMatrix = m_inverseViewMatrix * m_projection.InverseMatrix();
	m_frustum = eg::Frustum(m_inverseViewProjMatrix);
}

static int* physicsDebug = eg::TweakVarInt("phys_dbg_draw", 0, 0, 1);
static int* bloomLevels = eg::TweakVarInt("bloom_levels", 3, 0, 10);

void GameRenderer::Render(
	World& world, float gameTime, float dt, eg::FramebufferHandle outputFramebuffer, eg::Format outputFormat,
	uint32_t outputResX, uint32_t outputResY
)
{
	m_projection.SetResolution(static_cast<float>(outputResX), static_cast<float>(outputResY));

	const bool renderBlurredGlass = m_blurredTexturesNeeded && qvar::renderBlurredGlass(settings.lightingQuality);
	const uint32_t numWaterParticles = m_waterSimulator ? m_waterSimulator->NumParticles() : 0;
	const bool ssrEnabled = qvar::ssrLinearSamples(settings.reflectionsQuality) != 0 &&
	                        lightColorAttachmentFormat != eg::Format::R8G8B8A8_UNorm;

	if (settings.ssaoQuality == SSAOQuality::Off && m_ssao.has_value())
	{
		m_ssao = std::nullopt;
		m_deferredRenderer.SetSSAOTexture(std::nullopt);
	}

	if (!ssrEnabled && m_ssr.has_value())
		m_ssr = std::nullopt;

	bool updatePostProcessorInputs = false;

	const RenderTargetsCreateArgs renderTargetsCreateArgs = {
		.resX = outputResX,
		.resY = outputResY,
		.enableWater = numWaterParticles > 0,
		.enableSSR = ssrEnabled,
		.enableBlurredGlass = renderBlurredGlass,
	};
	if (!m_renderTargets.IsValid(renderTargetsCreateArgs))
	{
		m_renderTargets = RenderTargets();
		m_renderTargets = RenderTargets::Create(renderTargetsCreateArgs);

		m_deferredRenderer.RenderTargetsCreated(m_renderTargets);

		m_particleRenderer.SetDepthTexture(m_renderTargets.gbufferDepth);

		if (m_ssao.has_value())
		{
			m_ssao->RenderTargetsCreated(m_renderTargets);
			m_deferredRenderer.SetSSAOTexture(m_ssao->OutputTexture());
		}
		if (m_ssr.has_value())
			m_ssr->RenderTargetsCreated(m_renderTargets);
		if (m_waterRenderer.has_value())
			m_waterRenderer->RenderTargetsCreated(m_renderTargets);

		if (renderBlurredGlass)
		{
			m_glassBlurRenderer.MaybeUpdateResolution(outputResX, outputResY);
			m_renderTargets.glassBlurTexture = m_glassBlurRenderer.OutputTexture();
			m_renderTargets.glassBlurTextureDescriptorSet->BindTexture(*m_renderTargets.glassBlurTexture, 0);
		}

		updatePostProcessorInputs = true;

		eg::Log(eg::LogLevel::Info, "gfx", "Creating render targets @{0}x{1}", outputResX, outputResY);
	}

	if (ssrEnabled && (!m_ssr.has_value() || m_ssr->GetQualityLevel() != settings.reflectionsQuality))
	{
		m_ssr = SSR(settings.reflectionsQuality);
		m_ssr->RenderTargetsCreated(m_renderTargets);
	}

	if (settings.ssaoQuality != SSAOQuality::Off && !m_ssao.has_value())
	{
		m_ssao = SSAO();
		m_ssao->RenderTargetsCreated(m_renderTargets);
		m_deferredRenderer.SetSSAOTexture(m_ssao->OutputTexture());
	}

	if (m_lastSettingsGeneration != SettingsGeneration())
	{
		m_projection.SetFieldOfViewDeg(fovOverride.value_or(settings.fieldOfViewDeg));

		m_plShadowMapper.SetQuality(settings.shadowQuality);

		GravitySwitchVolLightMaterial::SetQuality(settings.lightingQuality);

		m_lastSettingsGeneration = SettingsGeneration();

		if (m_lightingQuality != settings.lightingQuality)
		{
			m_lightingQuality = settings.lightingQuality;

			// This was probably done before because bloom needs hdr?
			// m_bloomRenderTarget = nullptr;
		}
	}

	if (settings.bloomQuality != BloomQuality::Off)
	{
		bool outOfDate = m_bloomRenderTarget == nullptr || m_oldBloomQuality != settings.bloomQuality ||
		                 m_bloomRenderTarget->InputWidth() != outputResX ||
		                 m_bloomRenderTarget->InputHeight() != outputResY;

		if (outOfDate)
		{
			eg::Format bloomFormat = eg::Format::R8G8B8A8_UNorm;
			if (settings.bloomQuality == BloomQuality::High)
				bloomFormat = eg::Format::R16G16B16A16_Float;

			m_bloomRenderTarget =
				std::make_unique<eg::BloomRenderer::RenderTarget>(outputResX, outputResY, *bloomLevels, bloomFormat);

			if (m_bloomRenderer == nullptr || m_bloomRenderer->Format() != bloomFormat)
			{
				m_bloomRenderer = std::make_unique<eg::BloomRenderer>(bloomFormat);
			}

			m_oldBloomQuality = settings.bloomQuality;

			updatePostProcessorInputs = true;
		}
	}
	else if (m_bloomRenderTarget != nullptr)
	{
		updatePostProcessorInputs = true;
		m_bloomRenderTarget = nullptr;
	}

	if (updatePostProcessorInputs)
	{
		m_postProcessor.SetRenderTargets(m_renderTargets.finalLitTexture, m_bloomRenderTarget.get());
	}

	m_waterBarrierRenderer.Update(dt);

	if (m_particleManager && m_player)
	{
		m_particleManager->Step(dt, m_frustum, m_player->Rotation() * glm::vec3(0, 0, -1));
	}

	GravityCornerLightMaterial::instance.Update(dt);

	auto cpuTimerPrepare = eg::StartCPUTimer("Prepare Draw");

	RenderSettings::instance->data = {
		.viewProjection = m_viewProjMatrix,
		.invViewProjection = m_inverseViewProjMatrix,
		.viewMatrix = m_viewMatrix,
		.projectionMatrix = m_projection.Matrix(),
		.invViewMatrix = m_inverseViewMatrix,
		.invProjectionMatrix = m_projection.InverseMatrix(),
		.cameraPosition = glm::vec3(m_inverseViewMatrix[3]),
		.gameTime = gameTime,
		.renderResolution = glm::ivec2(outputResX, outputResY),
		.inverseRenderResolution = glm::vec2(1.0f / (float)outputResX, 1.0f / (float)outputResY),
	};
	RenderSettings::instance->UpdateBuffer();

	m_meshBatch.Begin();
	m_transparentMeshBatch.Begin();

	PrepareDrawArgs prepareDrawArgs;
	prepareDrawArgs.isEditor = false;
	prepareDrawArgs.meshBatch = &m_meshBatch;
	prepareDrawArgs.transparentMeshBatch = &m_transparentMeshBatch;
	prepareDrawArgs.player = m_player;
	prepareDrawArgs.frustum = &m_frustum;
	world.PrepareForDraw(prepareDrawArgs);

	m_transparentMeshBatch.End(eg::DC);
	m_meshBatch.End(eg::DC);

	cpuTimerPrepare.Stop();

	size_t numPointLightMeshBatchesUsed = 0;

	m_plShadowMapper.UpdateShadowMaps(
		[&](const PointLightShadowDrawArgs& args)
		{
			if (numPointLightMeshBatchesUsed == m_pointLightMeshBatches.size())
			{
				m_pointLightMeshBatches.emplace_back();
			}

			eg::MeshBatch& meshBatch = m_pointLightMeshBatches[numPointLightMeshBatchesUsed];
			meshBatch.Begin();

			EntGameDrawArgs entDrawArgs = {};
			entDrawArgs.world = &world;
			entDrawArgs.meshBatch = &meshBatch;
			entDrawArgs.shadowDrawArgs = &args;
			entDrawArgs.frustum = &args.frustum;

			if (args.renderStatic)
			{
				world.entManager.ForEachWithFlag(
					EntTypeFlags::ShadowDrawableS, [&](Ent& entity) { entity.GameDraw(entDrawArgs); }
				);
			}
			if (args.renderDynamic)
			{
				world.entManager.ForEachWithFlag(
					EntTypeFlags::ShadowDrawableD,
					[&](Ent& entity)
					{
						if (!(eg::HasFlag(entity.TypeFlags(), EntTypeFlags::ShadowDrawableS) && args.renderStatic))
						{
							entity.GameDraw(entDrawArgs);
						}
					}
				);
			}

			meshBatch.End(eg::DC);
		},
		[&](const PointLightShadowDrawArgs& args)
		{
			if (args.renderStatic)
			{
				world.DrawPointLightShadows(args);
			}

			MeshDrawArgs mDrawArgs;
			mDrawArgs.drawMode = MeshDrawMode::PointLightShadow;
			mDrawArgs.plShadowRenderArgs = &args;
			m_pointLightMeshBatches[numPointLightMeshBatchesUsed].Draw(eg::DC, &mDrawArgs);

			numPointLightMeshBatchesUsed++;
		},
		m_frustum
	);

	if (*physicsDebug && m_physicsDebugRenderer && m_physicsEngine)
	{
		m_physicsDebugRenderer->Prepare(*m_physicsEngine, m_viewProjMatrix);
	}

	MeshDrawArgs mDrawArgs;
	mDrawArgs.plShadowRenderArgs = nullptr;
	mDrawArgs.renderTargets = &m_renderTargets;

	{
		auto gpuTimerGeom = eg::StartGPUTimer("Geometry");
		auto cpuTimerGeom = eg::StartCPUTimer("Geometry");
		eg::DC.DebugLabelBegin("Geometry");

		m_deferredRenderer.BeginGeometry();

		world.Draw();
		mDrawArgs.drawMode = MeshDrawMode::Game;
		m_meshBatch.Draw(eg::DC, &mDrawArgs);

		eg::DC.EndRenderPass();

		m_renderTargets.gbufferColor1.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		m_renderTargets.gbufferColor2.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		m_renderTargets.gbufferDepth.UsageHint(eg::TextureUsage::DepthStencilReadOnly, eg::ShaderAccessFlags::Fragment);

		eg::DC.DebugLabelEnd();
	}

	if (numWaterParticles > 0)
	{
		auto gpuTimerWater = eg::StartGPUTimer("Water (early)");
		auto cpuTimerWater = eg::StartCPUTimer("Water (early)");
		eg::DC.DebugLabelBegin("Water (early)");
		m_waterRenderer->RenderEarly(m_renderTargets);
		m_renderTargets.waterDepthTexture.value().UsageHint(
			eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment
		);
		eg::DC.DebugLabelEnd();
	}

	if (m_ssao.has_value())
	{
		m_ssao->RunSSAOPass();
	}

	if (m_renderTargets.blurredGlassDepthStage.has_value())
	{
		auto gpuTimer = eg::StartGPUTimer("Blurred Glass Depth");
		eg::DC.DebugLabelBegin("Blurred Glass Depth");

		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = m_renderTargets.blurredGlassDepthStage->framebuffer.handle;
		rpBeginInfo.depthClearValue = 1;
		rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
		eg::DC.BeginRenderPass(rpBeginInfo);

		mDrawArgs.drawMode = MeshDrawMode::BlurredGlassDepthOnly;
		m_transparentMeshBatch.Draw(eg::DC, &mDrawArgs);

		eg::DC.EndRenderPass();

		m_renderTargets.blurredGlassDepthStage->outputTexture.UsageHint(
			eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment
		);

		eg::DC.DebugLabelEnd();
	}

	{
		auto gpuTimerLight = eg::StartGPUTimer("Lighting");
		auto cpuTimerLight = eg::StartCPUTimer("Lighting");
		eg::DC.DebugLabelBegin("Lighting");

		m_deferredRenderer.BeginLighting(m_renderTargets);

		m_deferredRenderer.DrawPointLights(m_pointLights, m_renderTargets, m_plShadowMapper.Resolution());

		eg::DC.DebugLabelEnd();
	}

	if (numWaterParticles > 0)
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (pre-water)");
		auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (pre-water)");

		eg::DC.DebugLabelBegin("Emissive");

		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_meshBatch.Draw(eg::DC, &mDrawArgs);

		eg::DC.DebugLabelEnd();
		eg::DC.DebugLabelBegin("Transparent (pre-water)");

		mDrawArgs.drawMode = MeshDrawMode::TransparentBeforeWater;
		m_transparentMeshBatch.Draw(eg::DC, &mDrawArgs);

		eg::DC.DebugLabelEnd();

		eg::DC.EndRenderPass();

		gpuTimerTransparent.Stop();
		cpuTimerTransparent.Stop();

		m_renderTargets.waterPostInput.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

		auto gpuTimerWater = eg::StartGPUTimer("Water (post)");
		auto cpuTimerWater = eg::StartCPUTimer("Water (post)");
		eg::DC.DebugLabelBegin("Water (post)");
		m_waterRenderer->RenderPost(m_renderTargets);
		eg::DC.DebugLabelEnd();
	}
	else
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Emissive");
		auto cpuTimerTransparent = eg::StartCPUTimer("Emissive");
		eg::DC.DebugLabelBegin("Emissive");

		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_meshBatch.Draw(eg::DC, &mDrawArgs);

		eg::DC.DebugLabelEnd();
	}

	if (m_ssr.has_value())
	{
		eg::DC.EndRenderPass();

		m_renderTargets.ssrColorInput.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

		auto gpuTimerSSR = eg::StartGPUTimer("SSR");
		eg::DC.DebugLabelBegin("SSR");

		SSR::SSRRenderArgs renderArgs = {
			.fallbackColor = m_ssrFallbackColor,
			.intensity = m_ssrIntensity,
			.renderTargets = &m_renderTargets,
		};

		if (settings.reflectionsQuality >= QualityLevel::High)
		{
			renderArgs.renderAdditionalCallback = [&]()
			{
				mDrawArgs.drawMode = MeshDrawMode::AdditionalSSR;
				m_transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
			};
		}

		m_ssr->Render(renderArgs);

		eg::DC.DebugLabelEnd();
	}

	if (renderBlurredGlass)
	{
		{
			auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (pre-blur)");
			auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (pre-blur)");
			eg::DC.DebugLabelBegin("Transparent (pre-blur)");

			mDrawArgs.drawMode = MeshDrawMode::TransparentBeforeBlur;
			m_transparentMeshBatch.Draw(eg::DC, &mDrawArgs);

			eg::DC.EndRenderPass();
			eg::DC.DebugLabelEnd();
		}

		m_renderTargets.finalLitTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

		{
			auto gpuTimerTransparent = eg::StartGPUTimer("Glass Blur");
			eg::DC.DebugLabelBegin("Glass Blur");

			m_glassBlurRenderer.Render(m_renderTargets.finalLitTextureDescriptorSet, 1.0f);

			eg::DC.DebugLabelEnd();
		}

		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = m_renderTargets.finalLitTextureFramebuffer.handle;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
		rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
		rpBeginInfo.depthStencilReadOnly = true;
		eg::DC.BeginRenderPass(rpBeginInfo);
	}

	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (final)");
		auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (final)");
		eg::DC.DebugLabelBegin("Transparent (final)");

		mDrawArgs.drawMode = MeshDrawMode::TransparentFinal;
		m_transparentMeshBatch.Draw(eg::DC, &mDrawArgs);

		eg::DC.DebugLabelEnd();
	}

	if (m_particleManager && m_particleManager->ParticlesToDraw() != 0)
	{
		m_particleRenderer.Draw(*m_particleManager);
	}

	if (*physicsDebug && m_physicsDebugRenderer && m_physicsEngine)
	{
		m_physicsDebugRenderer->Render();
	}

	eg::DC.EndRenderPass();

	if (world.playerHasGravityGun && m_gravityGun != nullptr)
	{
		m_gravityGun->Draw(m_renderTargets);
	}

	m_renderTargets.finalLitTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	eg::DC.DebugLabelBegin("Post");

	if (m_bloomRenderTarget != nullptr)
	{
		auto gpuTimerBloom = eg::StartGPUTimer("Bloom");
		auto cpuTimerBloom = eg::StartCPUTimer("Bloom");
		m_bloomRenderer->Render(glm::vec3(1.5f), m_renderTargets.finalLitTextureDescriptorSet, *m_bloomRenderTarget);
	}

	{
		auto gpuTimerPost = eg::StartGPUTimer("Post");
		auto cpuTimerPost = eg::StartCPUTimer("Post");
		m_postProcessor.Render(outputFramebuffer, outputFormat, postColorScale);
	}

	eg::DC.DebugLabelEnd();
}

std::pair<uint32_t, uint32_t> GameRenderer::GetScaledRenderResolution()
{
	uint32_t renderResolutionX =
		(uint32_t)(std::round((float)eg::CurrentResolutionX() * settings.renderResolutionScale));
	uint32_t renderResolutionY =
		(uint32_t)(std::round((float)eg::CurrentResolutionY() * settings.renderResolutionScale));
	return std::make_pair(renderResolutionX, renderResolutionY);
}
