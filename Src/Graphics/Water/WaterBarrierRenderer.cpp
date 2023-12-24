#include "WaterBarrierRenderer.hpp"

#include "../../World/World.hpp"
#include "IWaterSimulator.hpp"

#ifndef IOMOMI_ENABLE_WATER
WaterBarrierRenderer::WaterBarrierRenderer() {}
void WaterBarrierRenderer::Init(class IWaterSimulator* waterSimulator, class World& world) {}
void WaterBarrierRenderer::Update(float dt) {}
#else

static constexpr float TEXTURE_DENSITY = 2;
static constexpr uint32_t LOCAL_SIZE_X = 64;

static constexpr uint32_t LOCAL_SIZE_FADE = 8;

WaterBarrierRenderer::WaterBarrierRenderer()
{
	eg::ComputePipelineCreateInfo pipelineCI;
	pipelineCI.computeShader.shaderModule =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterBarrierDist.cs.glsl").DefaultVariant();
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	m_calcPipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.computeShader.shaderModule =
		eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterBarrierFade.cs.glsl").DefaultVariant();
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	m_fadePipeline = eg::Pipeline::Create(pipelineCI);

	eg::TextureCreateInfo defaultTextureCI;
	defaultTextureCI.width = 1;
	defaultTextureCI.height = 1;
	defaultTextureCI.format = eg::Format::R32_Float;
	defaultTextureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;
	defaultTextureCI.mipLevels = 1;
	m_defaultTexture = eg::Texture::Create2D(defaultTextureCI);

	eg::DC.ClearColorTexture(m_defaultTexture, 0, eg::Color(1, 1, 1));
	m_defaultTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

void WaterBarrierRenderer::Init(IWaterSimulator* waterSimulator, World& world)
{
	m_barriers.clear();

	world.entManager.ForEachOfType<GravityBarrierEnt>(
		[&](GravityBarrierEnt& entity)
		{
			if (waterSimulator == nullptr || !entity.RedFromWater())
			{
				entity.waterDistanceTexture = m_defaultTexture;
				return;
			}

			waterSimulator->EnableGravitiesGPUBuffer();

			Barrier& barrier = m_barriers.emplace_back();
			barrier.entity = std::dynamic_pointer_cast<GravityBarrierEnt>(entity.shared_from_this());

			const AxisAlignedQuadComp& aaQuadComp = *entity.GetComponentMut<AxisAlignedQuadComp>();
			float textureW = std::max(std::ceil(aaQuadComp.radius.x * 2 * TEXTURE_DENSITY), 2.0f);
			float textureH = std::max(std::ceil(aaQuadComp.radius.y * 2 * TEXTURE_DENSITY), 2.0f);

			eg::TextureCreateInfo textureCI;
			textureCI.width = static_cast<uint32_t>(textureW);
			textureCI.height = static_cast<uint32_t>(textureH);
			textureCI.format = eg::Format::R32_UInt;
			textureCI.flags =
				eg::TextureFlags::StorageImage | eg::TextureFlags::CopyDst | eg::TextureFlags::ManualBarrier;
			textureCI.mipLevels = 1;
			barrier.tmpTexture = eg::Texture::Create2D(textureCI);

			textureCI.format = eg::Format::R32_Float;
			textureCI.flags =
				eg::TextureFlags::ShaderSample | eg::TextureFlags::StorageImage | eg::TextureFlags::CopyDst;
			barrier.fadeTexture = eg::Texture::Create2D(textureCI);
			entity.waterDistanceTexture = barrier.fadeTexture;

			barrier.descriptorSetCalc = eg::DescriptorSet(m_calcPipeline, 0);
			barrier.descriptorSetCalc.BindStorageBuffer(
				waterSimulator->GetPositionsGPUBuffer(), 0, 0, waterSimulator->NumParticles() * sizeof(float) * 4);
			barrier.descriptorSetCalc.BindStorageBuffer(
				waterSimulator->GetGravitiesGPUBuffer(), 1, 0, waterSimulator->NumParticles());
			barrier.descriptorSetCalc.BindStorageImage(barrier.tmpTexture, 2);

			barrier.descriptorSetFade = eg::DescriptorSet(m_fadePipeline, 0);
			barrier.descriptorSetFade.BindStorageImage(barrier.tmpTexture, 0);
			barrier.descriptorSetFade.BindStorageImage(barrier.fadeTexture, 1);

			barrier.transform = entity.GetTransform() * glm::translate(glm::mat4(1), glm::vec3(-0.5f, -0.5f, 0.0f)) *
		                        glm::scale(glm::mat4(1), glm::vec3(1.0f / textureW, 1.0f / textureH, 1.0f));

			eg::DC.ClearColorTexture(barrier.fadeTexture, 0, eg::Color(1, 1, 1));
		});

	if (waterSimulator != nullptr)
	{
		m_numParticles = waterSimulator->NumParticles();
		m_dispatchCount = (waterSimulator->NumParticles() + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
	}
	else
	{
		m_numParticles = 0;
		m_dispatchCount = 0;
	}
}

static float* fadeSpeed = eg::TweakVarFloat("gb_water_fade_speed", 0.5f);

void WaterBarrierRenderer::Update(float dt)
{
	if (m_barriers.empty())
		return;

	eg::GPUTimer timer = eg::StartGPUTimer("Water Barrier Render");

	eg::DC.BindPipeline(m_calcPipeline);

	struct PushConstants
	{
		glm::mat4 inverseBarrierTransform;
		uint32_t blockedGravity;
	};

	for (Barrier& barrier : m_barriers)
	{
		eg::TextureBarrier preClearBarrier;
		preClearBarrier.oldUsage = eg::TextureUsage::Undefined;
		preClearBarrier.newUsage = eg::TextureUsage::CopyDst;
		eg::DC.Barrier(barrier.tmpTexture, preClearBarrier);

		eg::DC.ClearColorTexture(barrier.tmpTexture, 0, glm::ivec4(UINT32_MAX));

		eg::TextureBarrier postClearBarrier;
		postClearBarrier.oldUsage = eg::TextureUsage::CopyDst;
		postClearBarrier.newUsage = eg::TextureUsage::ILSReadWrite;
		postClearBarrier.newAccess = eg::ShaderAccessFlags::Compute;
		eg::DC.Barrier(barrier.tmpTexture, postClearBarrier);

		PushConstants pc;
		pc.inverseBarrierTransform = barrier.transform;
		pc.blockedGravity = barrier.entity->BlockedAxis();
		eg::DC.PushConstants(0, pc);

		eg::DC.BindDescriptorSet(barrier.descriptorSetCalc, 0);
		eg::DC.DispatchCompute(m_dispatchCount, barrier.tmpTexture.Width(), barrier.tmpTexture.Height());
	}

	eg::DC.BindPipeline(m_fadePipeline);

	float maxDelta = *fadeSpeed * dt;
	eg::DC.PushConstants(0, maxDelta);

	for (Barrier& barrier : m_barriers)
	{
		eg::TextureBarrier computeBarrier;
		computeBarrier.oldUsage = eg::TextureUsage::ILSReadWrite;
		computeBarrier.newUsage = eg::TextureUsage::ILSRead;
		computeBarrier.oldAccess = eg::ShaderAccessFlags::Compute;
		computeBarrier.newAccess = eg::ShaderAccessFlags::Compute;
		eg::DC.Barrier(barrier.tmpTexture, computeBarrier);

		barrier.fadeTexture.UsageHint(eg::TextureUsage::ILSReadWrite, eg::ShaderAccessFlags::Compute);

		eg::DC.BindDescriptorSet(barrier.descriptorSetFade, 0);
		eg::DC.DispatchCompute(
			(barrier.tmpTexture.Width() + LOCAL_SIZE_FADE - 1) / LOCAL_SIZE_FADE,
			(barrier.tmpTexture.Height() + LOCAL_SIZE_FADE - 1) / LOCAL_SIZE_FADE, 1);

		barrier.fadeTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}
}

#endif
