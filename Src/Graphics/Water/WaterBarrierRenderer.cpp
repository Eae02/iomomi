#include "WaterBarrierRenderer.hpp"
#include "WaterSimulator.hpp"
#include "../../World/World.hpp"

static constexpr float TEXTURE_DENSITY = 2;
static constexpr uint32_t LOCAL_SIZE_X = 64;

WaterBarrierRenderer::WaterBarrierRenderer()
{
	eg::ComputePipelineCreateInfo pipelineCI;
	pipelineCI.computeShader.shaderModule = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Water/WaterBarrierDist.cs.glsl").DefaultVariant();
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	m_pipeline = eg::Pipeline::Create(pipelineCI);
	
	eg::TextureCreateInfo defaultTextureCI;
	defaultTextureCI.width = 1;
	defaultTextureCI.height = 1;
	defaultTextureCI.format = eg::Format::R32_UInt;
	defaultTextureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;
	defaultTextureCI.mipLevels = 1;
	m_defaultTexture = eg::Texture::Create2D(defaultTextureCI);
	
	eg::DC.ClearColorTexture(m_defaultTexture, 0, glm::ivec4(UINT32_MAX));
	m_defaultTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

void WaterBarrierRenderer::Init(WaterSimulator& waterSimulator, World& world)
{
	m_barriers.clear();
	
	world.entManager.ForEachOfType<GravityBarrierEnt>([&] (GravityBarrierEnt& entity)
	{
		if (waterSimulator.NumParticles() == 0 || !entity.RedFromWater())
		{
			entity.waterDistanceTexture = m_defaultTexture;
			return;
		}
		
		waterSimulator.needParticleGravityBuffer = true;
		
		Barrier& barrier = m_barriers.emplace_back();
		barrier.entity = std::dynamic_pointer_cast<GravityBarrierEnt>(entity.shared_from_this());
		
		const AxisAlignedQuadComp& aaQuadComp = *entity.GetComponentMut<AxisAlignedQuadComp>();
		float textureW = std::max(std::ceil(aaQuadComp.radius.x * 2 * TEXTURE_DENSITY), 2.0f);
		float textureH = std::max(std::ceil(aaQuadComp.radius.y * 2 * TEXTURE_DENSITY), 2.0f);
		
		eg::TextureCreateInfo textureCI;
		textureCI.width = (uint32_t)textureW;
		textureCI.height = (uint32_t)textureH;
		textureCI.format = eg::Format::R32_UInt;
		textureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::StorageImage | eg::TextureFlags::CopyDst;
		textureCI.mipLevels = 1;
		barrier.texture = eg::Texture::Create2D(textureCI);
		entity.waterDistanceTexture = barrier.texture;
		
		barrier.descriptorSet = eg::DescriptorSet(m_pipeline, 0);
		barrier.descriptorSet.BindStorageBuffer(waterSimulator.GetPositionsBuffer(), 0, 0, waterSimulator.NumParticles() * sizeof(float) * 4);
		barrier.descriptorSet.BindStorageBuffer(waterSimulator.GetGravitiesBuffer(), 1, 0, waterSimulator.NumParticles());
		barrier.descriptorSet.BindStorageImage(barrier.texture, 2);
		
		barrier.transform =
			entity.GetTransform() *
			glm::translate(glm::mat4(1), glm::vec3(-0.5f, -0.5f, 0.0f)) *
			glm::scale(glm::mat4(1), glm::vec3(1.0f / textureW, 1.0f / textureH, 1.0f));
	});
	
	m_numParticles = waterSimulator.NumParticles();
	m_dispatchCount = (waterSimulator.NumParticles() + LOCAL_SIZE_X - 1) / LOCAL_SIZE_X;
}

void WaterBarrierRenderer::Update()
{
	if (m_barriers.empty())
		return;
	
	eg::GPUTimer timer = eg::StartGPUTimer("Water Barrier Render");
	
	eg::DC.BindPipeline(m_pipeline);
	
	struct PushConstants
	{
		glm::mat4 inverseBarrierTransform;
		uint32_t blockedGravity;
	};
	
	for (Barrier& barrier : m_barriers)
	{
		eg::DC.ClearColorTexture(barrier.texture, 0, glm::ivec4(UINT32_MAX));
		barrier.texture.UsageHint(eg::TextureUsage::ILSReadWrite, eg::ShaderAccessFlags::Compute);
		
		PushConstants pc;
		pc.inverseBarrierTransform = barrier.transform;
		pc.blockedGravity = barrier.entity->BlockedAxis();
		eg::DC.PushConstants(0, pc);
		
		eg::DC.BindDescriptorSet(barrier.descriptorSet, 0);
		eg::DC.DispatchCompute(m_dispatchCount, barrier.texture.Width(), barrier.texture.Height());
	}
	
	for (Barrier& barrier : m_barriers)
	{
		barrier.texture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
	}
}
