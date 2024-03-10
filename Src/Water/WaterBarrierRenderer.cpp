#include "WaterBarrierRenderer.hpp"
#include "WaterSimulationShaders.hpp"
#include "WaterSimulator.hpp"

#include "../World/World.hpp"

#include <glm/gtx/matrix_operation.hpp>

static constexpr float BARRIER_DISTANCE_TEXEL_SIZE = 0.5f; // The size of one texel in world space

static constexpr eg::Format BARRIER_DEPTH_FORMAT = eg::Format::Depth16;

WaterBarrierRenderer::WaterBarrierRenderer()
{
	if (!waterSimShaders.isWaterSupported)
		return;

	m_barrierPipeline = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
		.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterBarrier.vs.glsl").ToStageInfo(),
		.depthAttachmentFormat = BARRIER_DEPTH_FORMAT,
		.enableDepthTest = true,
		.enableDepthWrite = true,
		.depthCompare = eg::CompareOp::Less,
		.topology = eg::Topology::Points,
		.setBindModes = { eg::BindMode::DescriptorSet },
		.numColorAttachments = 0,
		.label = "WaterBarrier",
	});

	m_defaultTexture = eg::Texture::Create2D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst,
		.mipLevels = 1,
		.width = 1,
		.height = 1,
		.format = eg::Format::R32_Float,
	});

	const float clearValue = 1.0f;
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBufferWith<float>({ &clearValue, 1 });
	eg::DC.SetTextureData(
		m_defaultTexture, eg::TextureRange{ .sizeX = 1, .sizeY = 1, .sizeZ = 1 }, uploadBuffer.buffer,
		uploadBuffer.offset
	);

	m_defaultTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

void WaterBarrierRenderer::Init(const WaterSimulator2* simulator, World& world)
{
	m_barriers.clear();

	world.entManager.ForEachOfType<GravityBarrierEnt>(
		[&](GravityBarrierEnt& entity)
		{
			if (simulator == nullptr || !entity.RedFromWater())
			{
				entity.waterDistanceTexture = m_defaultTexture;
				return;
			}

			Barrier& barrier = m_barriers.emplace_back();
			barrier.entity = std::dynamic_pointer_cast<GravityBarrierEnt>(entity.shared_from_this());

			const AxisAlignedQuadComp& aaQuadComp = *entity.GetComponentMut<AxisAlignedQuadComp>();
			const float textureW = std::max(std::ceil(aaQuadComp.radius.x * 2 / BARRIER_DISTANCE_TEXEL_SIZE), 2.0f);
			const float textureH = std::max(std::ceil(aaQuadComp.radius.y * 2 / BARRIER_DISTANCE_TEXEL_SIZE), 2.0f);

			barrier.texture = eg::Texture::Create2D(eg::TextureCreateInfo{
				.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::FramebufferAttachment,
				.mipLevels = 1,
				.width = static_cast<uint32_t>(textureW),
				.height = static_cast<uint32_t>(textureH),
				.format = BARRIER_DEPTH_FORMAT,
			});

			entity.waterDistanceTexture = barrier.texture;

			barrier.framebuffer = eg::Framebuffer({}, eg::FramebufferAttachment(barrier.texture.handle));

			barrier.transform = glm::diagonal4x4(glm::vec4(2, -2, 1, 1)) * glm::inverse(entity.GetTransform());
		}
	);

	if (simulator != nullptr)
	{
		m_numParticles = simulator->NumParticles();

		m_descriptorSet = eg::DescriptorSet(m_barrierPipeline, 0);
		m_descriptorSet.BindStorageBuffer(
			simulator->GetPositionsGPUBuffer(), 0, 0, simulator->NumParticles() * sizeof(float) * 4
		);
	}
	else
	{
		m_numParticles = 0;
		m_descriptorSet.Destroy();
	}
}

static float* fadeSpeed = eg::TweakVarFloat("gb_water_fade_speed", 0.5f);

void WaterBarrierRenderer::Update(float dt)
{
	if (m_barriers.empty())
		return;

	eg::GPUTimer timer = eg::StartGPUTimer("Water Barrier Render");

	struct PushConstants
	{
		glm::mat4 inverseBarrierTransform;
		uint32_t blockedAxis;
	};

	for (Barrier& barrier : m_barriers)
	{
		eg::DC.BeginRenderPass(eg::RenderPassBeginInfo{
			.depthClearValue = 1.0f,
			.depthLoadOp = eg::AttachmentLoadOp::Clear,
			.framebuffer = barrier.framebuffer.handle,
		});

		eg::DC.BindPipeline(m_barrierPipeline);

		const PushConstants pc = {
			.inverseBarrierTransform = barrier.transform,
			.blockedAxis = static_cast<uint32_t>(barrier.entity->BlockedAxis()),
		};
		eg::DC.PushConstants(0, pc);

		eg::DC.BindDescriptorSet(m_descriptorSet, 0);
		eg::DC.Draw(0, m_numParticles, 0, 1);

		eg::DC.EndRenderPass();

		barrier.texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}
}
