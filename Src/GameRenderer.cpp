#include "GameRenderer.hpp"
#include "Settings.hpp"
#include "World/World.hpp"
#include "World/Entities/EntTypes/Visual/WindowEnt.hpp"
#include "World/Player.hpp"
#include "World/PrepareDrawArgs.hpp"
#include "World/GravityGun.hpp"
#include "Graphics/Materials/MeshDrawArgs.hpp"
#include "Graphics/Materials/GravitySwitchVolLightMaterial.hpp"
#include "Graphics/Materials/GravityCornerLightMaterial.hpp"

GameRenderer* GameRenderer::instance;

GameRenderer::GameRenderer(RenderContext& renderCtx)
	: m_renderCtx(&renderCtx), m_glassBlurRenderer(4, eg::Format::R16G16B16A16_Float)
{
	m_projection.SetZNear(0.02f);
	m_projection.SetZFar(200.0f);
}

void GameRenderer::WorldChanged(World& world)
{
	m_waterSimulator.Init(world);
	
	m_pointLights.clear();
	world.entManager.ForEach([&] (Ent& entity)
	{
		entity.CollectPointLights(m_pointLights);
	});
	if (world.playerHasGravityGun && m_gravityGun)
	{
		m_pointLights.push_back(m_gravityGun->light);
	}
	
	m_plShadowMapper.SetLightSources(m_pointLights);
	
	m_blurredTexturesNeeded = false;
	world.entManager.ForEachOfType<WindowEnt>([&] (const WindowEnt& windowEnt)
	{
		if (windowEnt.NeedsBlurredTextures())
			m_blurredTexturesNeeded = true;
	});
	
	ssrFallbackColor = world.ssrFallbackColor;
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
	m_viewMatrix = glm::lookAt(world.thumbnailCameraPos, world.thumbnailCameraPos + world.thumbnailCameraDir, glm::vec3(0, 1, 0));
	m_inverseViewMatrix = glm::inverse(m_viewMatrix);
	m_viewProjMatrix = m_projection.Matrix() * m_viewMatrix;
	m_inverseViewProjMatrix = m_inverseViewMatrix * m_projection.InverseMatrix();
	m_frustum = eg::Frustum(m_inverseViewProjMatrix);
}

static int* physicsDebug = eg::TweakVarInt("phys_dbg_draw", 0, 0, 1);

void GameRenderer::Render(World& world, float gameTime, float dt,
                          eg::FramebufferHandle outputFramebuffer, uint32_t outputResX, uint32_t outputResY)
{
	m_projection.SetResolution(outputResX, outputResY);
	m_rtManager.BeginFrame(outputResX, outputResY);
	
	if (settings.BloomEnabled())
	{
		bool outOfDate = m_bloomRenderTarget == nullptr ||
			m_bloomRenderTarget->InputWidth() != outputResX ||
			m_bloomRenderTarget->InputHeight() != outputResY;
		
		if (outOfDate)
		{
			m_bloomRenderTarget = std::make_unique<eg::BloomRenderer::RenderTarget>(
				outputResX, outputResY, 3);
			
			if (m_bloomRenderer == nullptr)
			{
				m_bloomRenderer = std::make_unique<eg::BloomRenderer>();
			}
		}
	}
	
	m_glassBlurRenderer.MaybeUpdateResolution(outputResX, outputResY);
	
	if (m_lastSettingsGeneration != SettingsGeneration())
	{
		m_projection.SetFieldOfViewDeg(settings.fieldOfViewDeg);
		
		m_plShadowMapper.SetQuality(settings.shadowQuality);
		
		GravitySwitchVolLightMaterial::SetQuality(settings.lightingQuality);
		
		m_lastSettingsGeneration = SettingsGeneration();
		
		if (m_lightingQuality != settings.lightingQuality)
		{
			m_lightingQuality = settings.lightingQuality;
			m_bloomRenderTarget.reset();
		}
	}
	
	if (m_particleManager && m_player)
	{
		m_particleManager->Step(dt, m_frustum, m_player->Rotation() * glm::vec3(0, 0, -1));
	}
	
	GravityCornerLightMaterial::instance.Update(dt);
	
	auto cpuTimerPrepare = eg::StartCPUTimer("Prepare Draw");
	
	RenderSettings::instance->gameTime = gameTime;
	RenderSettings::instance->cameraPosition = glm::vec3(m_inverseViewMatrix[3]);
	RenderSettings::instance->viewProjection = m_viewProjMatrix;
	RenderSettings::instance->invViewProjection = m_inverseViewProjMatrix;
	RenderSettings::instance->viewMatrix = m_viewMatrix;
	RenderSettings::instance->invViewMatrix = m_inverseViewMatrix;
	RenderSettings::instance->projectionMatrix = m_projection.Matrix();
	RenderSettings::instance->invProjectionMatrix = m_projection.InverseMatrix();
	RenderSettings::instance->UpdateBuffer();
	
	m_renderCtx->meshBatch.Begin();
	m_renderCtx->transparentMeshBatch.Begin();
	
	PrepareDrawArgs prepareDrawArgs;
	prepareDrawArgs.isEditor = false;
	prepareDrawArgs.meshBatch = &m_renderCtx->meshBatch;
	prepareDrawArgs.transparentMeshBatch = &m_renderCtx->transparentMeshBatch;
	prepareDrawArgs.player = m_player;
	prepareDrawArgs.frustum = &m_frustum;
	world.PrepareForDraw(prepareDrawArgs);
	if (world.playerHasGravityGun && m_gravityGun != nullptr)
	{
		m_gravityGun->Draw(m_renderCtx->meshBatch, m_renderCtx->transparentMeshBatch);
	}
	
	m_renderCtx->transparentMeshBatch.End(eg::DC);
	m_renderCtx->meshBatch.End(eg::DC);
	
	cpuTimerPrepare.Stop();
	
	size_t numPointLightMeshBatchesUsed = 0;
	
	m_plShadowMapper.UpdateShadowMaps([&] (const PointLightShadowDrawArgs& args)
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
			world.entManager.ForEachWithFlag(EntTypeFlags::ShadowDrawableS, [&] (Ent& entity)
			{
				entity.GameDraw(entDrawArgs);
			});
		}
		if (args.renderDynamic)
		{
			world.entManager.ForEachWithFlag(EntTypeFlags::ShadowDrawableD, [&] (Ent& entity)
			{
				if (!(eg::HasFlag(entity.TypeFlags(), EntTypeFlags::ShadowDrawableS) && args.renderStatic))
				{
					entity.GameDraw(entDrawArgs);
				}
			});
		}
		
		meshBatch.End(eg::DC);
	},
	[&] (const PointLightShadowDrawArgs& args)
	{
		if (args.renderStatic)
		{
			world.DrawPointLightShadows(args);
		}
		
		MeshDrawArgs mDrawArgs;
		mDrawArgs.drawMode = MeshDrawMode::PointLightShadow;
		mDrawArgs.plShadowRenderArgs = &args;
		mDrawArgs.rtManager = nullptr;
		m_pointLightMeshBatches[numPointLightMeshBatchesUsed].Draw(eg::DC, &mDrawArgs);
		
		numPointLightMeshBatchesUsed++;
	}, m_frustum);
	
	const bool renderBlurredGlass = m_blurredTexturesNeeded && settings.lightingQuality >= QualityLevel::Medium;
	
	MeshDrawArgs mDrawArgs;
	mDrawArgs.rtManager = &m_rtManager;
	mDrawArgs.plShadowRenderArgs = nullptr;
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		mDrawArgs.waterDepthTexture = m_rtManager.GetRenderTexture(RenderTex::WaterDepthBlurred2);
	}
	else
	{
		mDrawArgs.waterDepthTexture = WaterRenderer::GetDummyDepthTexture();
		m_rtManager.RedirectRenderTexture(RenderTex::LitWithoutWater, RenderTex::LitWithoutSSR);
	}
	
	if (!settings.SSREnabled())
	{
		m_rtManager.RedirectRenderTexture(RenderTex::LitWithoutSSR, RenderTex::Lit);
	}
	
	{
		auto gpuTimerGeom = eg::StartGPUTimer("Geometry");
		auto cpuTimerGeom = eg::StartCPUTimer("Geometry");
		eg::DC.DebugLabelBegin("Geometry");
		
		m_renderCtx->renderer.BeginGeometry(m_rtManager);
		
		world.Draw();
		mDrawArgs.drawMode = MeshDrawMode::Game;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		eg::DC.DebugLabelEnd();
		eg::DC.DebugLabelBegin("Geometry Flags");
		
		m_renderCtx->renderer.BeginGeometryFlags(m_rtManager);
		
		mDrawArgs.drawMode = MeshDrawMode::ObjectFlags;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_renderCtx->renderer.EndGeometry(m_rtManager);
		eg::DC.DebugLabelEnd();
	}
	
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		auto gpuTimerWater = eg::StartGPUTimer("Water (early)");
		auto cpuTimerWater = eg::StartCPUTimer("Water (early)");
		eg::DC.DebugLabelBegin("Water (early)");
		m_renderCtx->waterRenderer.RenderEarly(m_waterSimulator.GetPositionsBuffer(), m_waterSimulator.NumParticlesToDraw(), m_rtManager);
		eg::DC.DebugLabelEnd();
	}
	
	{
		auto gpuTimerLight = eg::StartGPUTimer("Lighting");
		auto cpuTimerLight = eg::StartCPUTimer("Lighting");
		eg::DC.DebugLabelBegin("Lighting");
		
		m_renderCtx->renderer.BeginLighting(m_rtManager);
		
		m_renderCtx->renderer.DrawPointLights(m_pointLights, mDrawArgs.waterDepthTexture, m_rtManager, m_plShadowMapper.Resolution());
		
		m_renderCtx->renderer.End();
		eg::DC.DebugLabelEnd();
	}
	
	if (m_waterSimulator.NumParticlesToDraw() > 0)
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (pre-water)");
		auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (pre-water)");
		
		m_renderCtx->renderer.BeginTransparent(RenderTex::LitWithoutWater, m_rtManager);
		
		eg::DC.DebugLabelBegin("Emissive");
		
		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		eg::DC.DebugLabelEnd();
		eg::DC.DebugLabelBegin("Transparent (pre-water)");
		
		mDrawArgs.drawMode = MeshDrawMode::TransparentBeforeWater;
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
		
		eg::DC.DebugLabelEnd();
		
		m_renderCtx->renderer.EndTransparent();
		m_rtManager.RenderTextureUsageHintFS(RenderTex::GBDepth);
		
		gpuTimerTransparent.Stop();
		cpuTimerTransparent.Stop();
		
		m_rtManager.RenderTextureUsageHintFS(RenderTex::LitWithoutWater);
		
		auto gpuTimerWater = eg::StartGPUTimer("Water (post)");
		auto cpuTimerWater = eg::StartCPUTimer("Water (post)");
		eg::DC.DebugLabelBegin("Water (post)");
		m_renderCtx->waterRenderer.RenderPost(m_rtManager);
		eg::DC.DebugLabelEnd();
	}
	else
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Emissive");
		auto cpuTimerTransparent = eg::StartCPUTimer("Emissive");
		eg::DC.DebugLabelBegin("Emissive");
		
		m_renderCtx->renderer.BeginTransparent(RenderTex::LitWithoutSSR, m_rtManager);
		
		mDrawArgs.drawMode = MeshDrawMode::Emissive;
		m_renderCtx->meshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_renderCtx->renderer.EndTransparent();
		m_rtManager.RenderTextureUsageHintFS(RenderTex::GBDepth);
		
		eg::DC.DebugLabelEnd();
	}
	
	if (settings.SSREnabled())
	{
		m_rtManager.RenderTextureUsageHintFS(RenderTex::LitWithoutSSR);
		
		auto gpuTimerTransparent = eg::StartGPUTimer("SSR");
		eg::DC.DebugLabelBegin("SSR");
		
		m_renderCtx->ssr.Render(mDrawArgs.waterDepthTexture, RenderTex::Lit, m_rtManager, ssrFallbackColor);
		
		eg::DC.DebugLabelEnd();
	}
	
	if (renderBlurredGlass)
	{
		{
			auto gpuTimerTransparent = eg::StartGPUTimer("Blurred Glass DO");
			eg::DC.DebugLabelBegin("Blurred Glass DO");
			
			eg::RenderPassBeginInfo rpBeginInfo;
			rpBeginInfo.framebuffer = m_rtManager.GetFramebuffer({}, {}, RenderTex::BlurredGlassDepth, "BlurredGlassDO");
			rpBeginInfo.depthClearValue = 1;
			rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Clear;
			eg::DC.BeginRenderPass(rpBeginInfo);
			
			mDrawArgs.drawMode = MeshDrawMode::BlurredGlassDepthOnly;
			m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
			
			eg::DC.EndRenderPass();
			
			m_rtManager.RenderTextureUsageHintFS(RenderTex::BlurredGlassDepth);
			
			eg::DC.DebugLabelEnd();
		}
		
		{
			auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (pre-blur)");
			auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (pre-blur)");
			eg::DC.DebugLabelBegin("Transparent (pre-blur)");
			
			eg::TextureRange srcRange = { };
			srcRange.sizeX = outputResX;
			srcRange.sizeY = outputResY;
			srcRange.sizeZ = 1;
			
			m_rtManager.RenderTextureUsageHint(RenderTex::Lit, eg::TextureUsage::CopySrc, eg::ShaderAccessFlags::None);
			eg::DC.CopyTexture(m_rtManager.GetRenderTexture(RenderTex::Lit), m_rtManager.GetRenderTexture(RenderTex::LitWithoutBlurredGlass),
				srcRange, eg::TextureOffset());
			
			m_renderCtx->renderer.BeginTransparent(RenderTex::LitWithoutBlurredGlass, m_rtManager);
			
			mDrawArgs.drawMode = MeshDrawMode::TransparentBeforeBlur;
			m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
			
			m_renderCtx->renderer.EndTransparent();
			eg::DC.DebugLabelEnd();
		}
		
		{
			auto gpuTimerTransparent = eg::StartGPUTimer("Glass Blur");
			eg::DC.DebugLabelBegin("Glass Blur");
			
			m_rtManager.RenderTextureUsageHintFS(RenderTex::LitWithoutBlurredGlass);
			m_glassBlurRenderer.Render(m_rtManager.GetRenderTexture(RenderTex::LitWithoutBlurredGlass));
			mDrawArgs.glassBlurTexture = m_glassBlurRenderer.OutputTexture();
			
			eg::DC.DebugLabelEnd();
		}
	}
	
	{
		auto gpuTimerTransparent = eg::StartGPUTimer("Transparent (final)");
		auto cpuTimerTransparent = eg::StartCPUTimer("Transparent (final)");
		eg::DC.DebugLabelBegin("Transparent (final)");
		
		m_renderCtx->renderer.BeginTransparent(RenderTex::Lit, m_rtManager);
		
		mDrawArgs.drawMode = MeshDrawMode::TransparentFinal;
		m_renderCtx->transparentMeshBatch.Draw(eg::DC, &mDrawArgs);
		
		m_renderCtx->renderer.EndTransparent();
		eg::DC.DebugLabelEnd();
	}
	
	if (m_particleManager && m_particleManager->ParticlesToDraw() != 0)
	{
		m_rtManager.RenderTextureUsageHintFS(RenderTex::GBDepth);
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.framebuffer = m_rtManager.GetFramebuffer(RenderTex::Lit, {}, {}, "Particles");
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
		eg::DC.BeginRenderPass(rpBeginInfo);
		
		m_renderCtx->particleRenderer.Draw(*m_particleManager, m_rtManager.GetRenderTexture(RenderTex::GBDepth));
		
		eg::DC.EndRenderPass();
	}
	
	//TODO: Move gun rendering here
	
	if (*physicsDebug && m_physicsDebugRenderer && m_physicsEngine)
	{
		m_physicsDebugRenderer->Render(*m_physicsEngine, m_viewProjMatrix,
			m_rtManager.GetRenderTexture(RenderTex::Lit), m_rtManager.GetRenderTexture(RenderTex::GBDepth));
	}
	
	m_rtManager.RenderTextureUsageHintFS(RenderTex::Lit);
	
	eg::DC.DebugLabelBegin("Post");
	
	if (m_bloomRenderTarget != nullptr)
	{
		auto gpuTimerBloom = eg::StartGPUTimer("Bloom");
		auto cpuTimerBloom = eg::StartCPUTimer("Bloom");
		m_bloomRenderer->Render(glm::vec3(1.5f), m_rtManager.GetRenderTexture(RenderTex::Lit), *m_bloomRenderTarget);
	}
	
	{
		auto gpuTimerPost = eg::StartGPUTimer("Post");
		auto cpuTimerPost = eg::StartCPUTimer("Post");
		m_renderCtx->postProcessor.Render(m_rtManager.GetRenderTexture(RenderTex::Lit), m_bloomRenderTarget.get(),
		                                  outputFramebuffer, outputResX, outputResY, postColorScale);
	}
	
	eg::DC.DebugLabelEnd();
}
