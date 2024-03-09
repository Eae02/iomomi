#include "WaterSimulator.hpp"
#include "../Graphics/GraphicsCommon.hpp"
#include "../World/VoxelBuffer.hpp"
#include "WaterConstants.hpp"
#include "WaterGenerate.hpp"
#include "WaterSimulationShaders.hpp"
#include "WaterSimulationThread.hpp"

#include <bit>
#include <cstring>

static uint32_t RoundToNextPowerOf2(uint32_t v)
{
	uint32_t p = 1;
	while (p < v)
		p *= 2;
	return p;
}

static std::uniform_real_distribution<float> radiusDist(W_MIN_PARTICLE_RADIUS, W_MAX_PARTICLE_RADIUS);

struct SortFarIndexBuffer
{
	std::vector<uint32_t> indices;
	std::vector<uint32_t> firstIndex;
};

static SortFarIndexBuffer ComputeSortFarIndexBuffer(uint32_t numParticlesTwoPow)
{
	uint32_t workGroupSize = waterSimShaders.sortWorkGroupSize;

	SortFarIndexBuffer result;

	for (uint32_t j = workGroupSize; j < numParticlesTwoPow; j <<= 1)
	{
		result.firstIndex.push_back(result.indices.size());
		for (uint32_t groupI0 = 0; groupI0 < numParticlesTwoPow; groupI0 += workGroupSize)
		{
			if ((groupI0 & j) == 0)
				result.indices.push_back(groupI0);
		}
	}

	result.firstIndex.push_back(result.indices.size());

	return result;
}

static constexpr eg::BufferFlags STAGING_BUFFER_FLAGS = eg::BufferFlags::HostAllocate | eg::BufferFlags::MapWrite |
                                                        eg::BufferFlags::CopySrc | eg::BufferFlags::ManualBarrier;

static glm::vec3 RandomPointOnUnitSphere(pcg32_fast& rng)
{
	std::uniform_real_distribution<float> twoPiDist(0, eg::TWO_PI);
	std::uniform_real_distribution<float> zeroOneDist(0, 1);

	float a = std::uniform_real_distribution<float>(0, eg::TWO_PI)(rng);
	float z = std::uniform_real_distribution<float>(-1, 1)(rng);
	float l = std::sqrt(1.0f - z * z);

	return glm::vec3(l * std::cos(a), l * std::sin(a), z);
}

static eg::Buffer CreateRandomOffsetsBuffer(pcg32_fast& rng, eg::CommandContext& cc)
{
	constexpr uint32_t NUM_RANDOM_OFFSETS = 256;
	constexpr uint32_t BUFFER_SIZE = NUM_RANDOM_OFFSETS * sizeof(uint32_t);

	eg::Buffer randomOffsetsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = sizeof(uint32_t) * BUFFER_SIZE,
	});

	eg::Buffer uploadBuffer = eg::Buffer(STAGING_BUFFER_FLAGS, BUFFER_SIZE, nullptr);
	volatile uint32_t* data = static_cast<uint32_t*>(uploadBuffer.Map());

	for (uint32_t i = 0; i < NUM_RANDOM_OFFSETS; i++)
	{
		glm::vec3 offset = RandomPointOnUnitSphere(rng);
		data[i] = glm::packSnorm4x8(glm::vec4(offset, 0.0f));
	}

	uploadBuffer.Flush();

	cc.CopyBuffer(uploadBuffer, randomOffsetsBuffer, 0, 0, BUFFER_SIZE);

	cc.Barrier(
		randomOffsetsBuffer,
		eg::BufferBarrier{
			.oldUsage = eg::BufferUsage::CopyDst,
			.newUsage = eg::BufferUsage::UniformBuffer,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	return randomOffsetsBuffer;
}

void WaterSimulator2::SetBufferData(eg::CommandContext& cc, eg::BufferRef buffer, uint64_t dataSize, const void* data)
{
	eg::Buffer uploadBuffer = eg::Buffer(STAGING_BUFFER_FLAGS, dataSize, nullptr);
	std::memcpy(uploadBuffer.Map(0, dataSize), data, dataSize);
	uploadBuffer.Flush(0, dataSize);

	cc.CopyBuffer(uploadBuffer, buffer, 0, 0, dataSize);

	cc.Barrier(
		buffer,
		eg::BufferBarrier{
			.oldUsage = eg::BufferUsage::CopyDst,
			.newUsage = eg::BufferUsage::StorageBufferReadWrite,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);
}

WaterSimulator2::WaterSimulator2(const WaterSimulatorInitArgs& args, eg::CommandContext& cc) : m_rng(time(nullptr))
{
	EG_ASSERT((args.positions.size() % NUM_PARTICLES_ALIGNMENT) == 0);
	EG_ASSERT((args.positions.size() % waterSimShaders.commonWorkGroupSize) == 0);

	eg::BufferFlags downloadFlags = {};
	if (args.enableDataDownload)
		downloadFlags |= eg::BufferFlags::CopySrc;

	auto [minBounds, maxBounds] = args.voxelBuffer->CalculateBounds();

	m_voxelMinBounds = minBounds;
	m_voxelMaxBounds = maxBounds;

	m_backgroundThread.Initialize(minBounds, args.positions.size());

	m_numParticles = args.positions.size();
	m_numParticlesTwoPow = RoundToNextPowerOf2(std::max(m_numParticles, waterSimShaders.sortWorkGroupSize));

	m_sortNumWorkGroups = m_numParticlesTwoPow / waterSimShaders.sortWorkGroupSize;

	SortFarIndexBuffer sortFarIndexBuffer = ComputeSortFarIndexBuffer(m_numParticlesTwoPow);
	m_farSortIndexBufferOffsets = std::move(sortFarIndexBuffer.firstIndex);

	m_collisionData = WaterCollisionData::Create(cc, *args.voxelBuffer, std::move(args.collisionQuads));

	const uint32_t positionsDataSize = sizeof(float) * 4 * m_numParticles;
	const uint32_t particleDataBufferSize = positionsDataSize * 4;

	// ** Creates buffers **
	m_particleDataBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = particleDataBufferSize,
		.label = "WaterParticleData",
	});

	m_particleDataForCPUBufferBytesPerFrame = sizeof(uint32_t) * 2 * m_numParticles;
	m_particleDataForCPUBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | eg::BufferFlags::Download |
	             eg::BufferFlags::MapRead,
		.size = m_particleDataForCPUBufferBytesPerFrame * WaterSimulationThread::FRAME_CYCLE_LENGTH,
		.label = "ParticleDataDownload",
	});
	m_particleDataForCPUMappedMemory = static_cast<glm::uvec2*>(m_particleDataForCPUBuffer.Map());

	m_gravityChangeBitsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | eg::BufferFlags::CopyDst,
		.size = (m_numParticles / 32) * sizeof(uint32_t),
		.label = "ParticleChangeBits",
	});

	m_densityBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = m_numParticles * sizeof(float) * 2,
		.label = "DensityData",
	});

	uint32_t sortedByCellBufferSize = m_numParticlesTwoPow * sizeof(uint32_t) * 2;
	m_sortedByCellBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = sortedByCellBufferSize,
		.label = "WaterSortedByCell",
	});

	m_cellNumOctGroupsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = sizeof(uint32_t) * m_numParticles,
	});

	const uint64_t octGroupRangesBufferSize = sizeof(uint32_t) * m_numParticles * W_MAX_OCT_GROUPS_PER_CELL;
	m_octGroupRangesBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags =
			eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = octGroupRangesBufferSize,
	});

	if (args.enableDataDownload)
	{
		cc.FillBuffer(m_octGroupRangesBuffer, 0, octGroupRangesBufferSize, 0xcc);
	}

	m_totalNumOctGroupsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::IndirectCommands |
	             eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = sizeof(uint32_t) * 3,
	});

	const uint32_t totalNumOctGroupsInitialData[3] = { 0, 1, 1 };
	SetBufferData(cc, m_totalNumOctGroupsBuffer, sizeof(totalNumOctGroupsInitialData), totalNumOctGroupsInitialData);

	m_randomOffsetsBuffer = CreateRandomOffsetsBuffer(m_rng, cc);

	// Creates m_numOctGroupsPrefixSumLayerOffsets
	uint32_t numOctGroupsPrefixSumLayerOffset = 0;
	uint32_t numOctGroupsPrefixSumCurrentLayerLength = m_numParticles / waterSimShaders.commonWorkGroupSize;
	for (uint32_t i = 0; i < 4; i++)
	{
		if (numOctGroupsPrefixSumCurrentLayerLength == 0)
		{
			m_numOctGroupsPrefixSumLayerOffsets[i] = UINT32_MAX;
			m_numOctGroupsPrefixSumLayerLengths[i] = 0;
		}
		else
		{
			m_numOctGroupsPrefixSumLayerOffsets[i] = numOctGroupsPrefixSumLayerOffset;
			m_numOctGroupsPrefixSumLayerLengths[i] = numOctGroupsPrefixSumCurrentLayerLength;

			numOctGroupsPrefixSumLayerOffset += numOctGroupsPrefixSumCurrentLayerLength;
			if ((numOctGroupsPrefixSumCurrentLayerLength % waterSimShaders.commonWorkGroupSize) != 0)
				numOctGroupsPrefixSumLayerOffset++;

			numOctGroupsPrefixSumCurrentLayerLength /= waterSimShaders.commonWorkGroupSize;
		}
	}

	m_cellNumOctGroupsPrefixSumBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = sizeof(uint32_t) * numOctGroupsPrefixSumLayerOffset,
	});

	// Creates the far sort index buffer and uploads index buffer data
	const uint64_t indexBufferSize = sortFarIndexBuffer.indices.size() * sizeof(uint32_t);
	if (!sortFarIndexBuffer.indices.empty())
	{
		m_farSortIndexBuffer = eg::Buffer(eg::BufferCreateInfo{
			.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
			.size = indexBufferSize,
		});

		SetBufferData(cc, m_farSortIndexBuffer, indexBufferSize, sortFarIndexBuffer.indices.data());
	}

	glm::uvec3 cellOffsetsTextureSize = glm::ceil(glm::vec3(maxBounds - minBounds + 1) / W_INFLUENCE_RADIUS);
	m_cellOffsetsTexture = eg::Texture::Create3D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::StorageImage | eg::TextureFlags::CopyDst | eg::TextureFlags::ManualBarrier,
		.mipLevels = 1,
		.width = cellOffsetsTextureSize.x,
		.height = cellOffsetsTextureSize.y,
		.depth = cellOffsetsTextureSize.z,
		.format = eg::Format::R16_UInt,
	});

	// ** Creates descriptor sets **

	m_sortInitialDS = eg::DescriptorSet(waterSimShaders.sortPipelineInitial, 0);
	m_sortInitialDS.BindStorageBuffer(m_particleDataBuffer, 0);
	m_sortInitialDS.BindStorageBuffer(m_sortedByCellBuffer, 1);

	m_sortNearDS = eg::DescriptorSet(waterSimShaders.sortPipelineNear, 0);
	m_sortNearDS.BindStorageBuffer(m_sortedByCellBuffer, 0);

	if (!sortFarIndexBuffer.indices.empty())
	{
		m_sortFarDS = eg::DescriptorSet(waterSimShaders.sortPipelineFar, 0);
		m_sortFarDS.BindStorageBuffer(m_sortedByCellBuffer, 0);
		m_sortFarDS.BindStorageBuffer(m_farSortIndexBuffer, 1);
	}

	m_detectCellEdgesDS = eg::DescriptorSet(waterSimShaders.detectCellEdges, 0);
	m_detectCellEdgesDS.BindStorageBuffer(m_sortedByCellBuffer, 0);
	m_detectCellEdgesDS.BindStorageBuffer(m_particleDataBuffer, 1);
	m_detectCellEdgesDS.BindStorageBuffer(m_cellNumOctGroupsBuffer, 2);
	m_detectCellEdgesDS.BindStorageBuffer(m_cellNumOctGroupsPrefixSumBuffer, 3);
	m_detectCellEdgesDS.BindStorageImage(m_cellOffsetsTexture, 4);
	m_detectCellEdgesDS.BindStorageBuffer(
		m_particleDataForCPUBuffer, 5, eg::BIND_BUFFER_OFFSET_DYNAMIC, m_particleDataForCPUBufferBytesPerFrame
	);

	m_octGroupsPrefixSumDS = eg::DescriptorSet(waterSimShaders.octGroupsPrefixSum, 0);
	m_octGroupsPrefixSumDS.BindStorageBuffer(m_cellNumOctGroupsPrefixSumBuffer, 0);

	m_writeOctGroupsDS = eg::DescriptorSet(waterSimShaders.writeOctGroups, 0);
	m_writeOctGroupsDS.BindStorageBuffer(m_cellNumOctGroupsPrefixSumBuffer, 0);
	m_writeOctGroupsDS.BindStorageBuffer(m_cellNumOctGroupsBuffer, 1);
	m_writeOctGroupsDS.BindStorageBuffer(m_octGroupRangesBuffer, 2);
	m_writeOctGroupsDS.BindStorageBuffer(m_totalNumOctGroupsBuffer, 3);

	static const eg::DescriptorSetBinding ForEachNearDescriptorBindings[] = {
		{
			.binding = 0,
			.type = eg::BindingType::StorageBuffer,
			.shaderAccess = eg::ShaderAccessFlags::Compute,
			.rwMode = eg::ReadWriteMode::ReadOnly,
		},
		{
			.binding = 1,
			.type = eg::BindingType::StorageBuffer,
			.shaderAccess = eg::ShaderAccessFlags::Compute,
			.rwMode = eg::ReadWriteMode::ReadOnly,
		},
		{
			.binding = 2,
			.type = eg::BindingType::StorageImage,
			.shaderAccess = eg::ShaderAccessFlags::Compute,
			.rwMode = eg::ReadWriteMode::ReadOnly,
		},
	};
	m_forEachNearDS = eg::DescriptorSet(ForEachNearDescriptorBindings);
	m_forEachNearDS.BindStorageBuffer(m_octGroupRangesBuffer, 0);
	m_forEachNearDS.BindStorageBuffer(m_particleDataBuffer, 1);
	m_forEachNearDS.BindStorageImage(m_cellOffsetsTexture, 2);

	m_calcDensityDS1 = eg::DescriptorSet(waterSimShaders.calculateDensity, 1);
	m_calcDensityDS1.BindStorageBuffer(m_densityBuffer, 0);

	m_calcAccelDS1 = eg::DescriptorSet(waterSimShaders.calculateAcceleration, 1);
	m_calcAccelDS1.BindStorageBuffer(m_densityBuffer, 0);
	m_calcAccelDS1.BindUniformBuffer(m_randomOffsetsBuffer, 1);

	m_moveAndCollisionDS = eg::DescriptorSet(waterSimShaders.moveAndCollision, 0);
	m_moveAndCollisionDS.BindStorageBuffer(m_particleDataBuffer, 0);
	m_moveAndCollisionDS.BindTexture(m_collisionData.voxelDataTexture, 1, &framebufferNearestSampler);
	m_moveAndCollisionDS.BindStorageBuffer(m_collisionData.collisionQuadsBuffer, 2);

	m_gravityChangeDS = eg::DescriptorSet(waterSimShaders.changeGravity, 0);
	m_gravityChangeDS.BindStorageBuffer(
		m_particleDataBuffer, 0, positionsDataSize * 2, positionsDataSize
	); // offset for velocities
	m_gravityChangeDS.BindStorageBuffer(m_gravityChangeBitsBuffer, 1);

	// ** Uploads data **

	// Uploads positions into the positions buffer and generates particle radii
	eg::Buffer particleDataUploadBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::HostAllocate | eg::BufferFlags::MapWrite | eg::BufferFlags::CopySrc |
	             eg::BufferFlags::ManualBarrier,
		.size = particleDataBufferSize,
	});
	float* positionsUploadMemory = static_cast<float*>(particleDataUploadBuffer.Map());
	std::memset(positionsUploadMemory, 0, particleDataBufferSize);
	uint32_t* dataBitsUploadMemory = static_cast<uint32_t*>(particleDataUploadBuffer.Map()) + m_numParticles * 8 + 3;
	for (size_t i = 0; i < args.positions.size(); i++)
	{
		positionsUploadMemory[i * 4 + 0] = args.positions[i].x;
		positionsUploadMemory[i * 4 + 1] = args.positions[i].y;
		positionsUploadMemory[i * 4 + 2] = args.positions[i].z;

		// Sets the data bits so that the gravity is -Y and the persistent id is the index of the particle
		dataBitsUploadMemory[i * 4] = static_cast<uint32_t>(Dir::NegY) | (static_cast<uint32_t>(i) << 16);
	}
	particleDataUploadBuffer.Flush();
	cc.CopyBuffer(particleDataUploadBuffer, m_particleDataBuffer, 0, 0, particleDataBufferSize);

	cc.Barrier(
		m_particleDataBuffer,
		eg::BufferBarrier{
			.oldUsage = eg::BufferUsage::CopyDst,
			.newUsage = eg::BufferUsage::StorageBufferRead,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	cc.Barrier(
		m_cellOffsetsTexture,
		{
			.newUsage = eg::TextureUsage::ILSWrite,
			.newAccess = eg::ShaderAccessFlags::Compute,
			.subresource = eg::TextureSubresource(),
		}
	);
}

static const eg::BufferBarrier computeToComputeBarrier = {
	.oldUsage = eg::BufferUsage::StorageBufferReadWrite,
	.newUsage = eg::BufferUsage::StorageBufferReadWrite,
	.oldAccess = eg::ShaderAccessFlags::Compute,
	.newAccess = eg::ShaderAccessFlags::Compute,
};

void WaterSimulator2::RunSortPhase(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterSort");

	const uint32_t sortInitialPushConstants[4] = {
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.x)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.y)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.z)),
		m_numParticles,
	};
	cc.BindPipeline(waterSimShaders.sortPipelineInitial);
	cc.BindDescriptorSet(m_sortInitialDS, 0);
	cc.PushConstants(0, sizeof(sortInitialPushConstants), sortInitialPushConstants);
	cc.DispatchCompute(m_sortNumWorkGroups, 1, 1);

	for (uint32_t k = waterSimShaders.sortWorkGroupSize << 1; k <= m_numParticlesTwoPow; k <<= 1)
	{
		cc.BindPipeline(waterSimShaders.sortPipelineFar);
		cc.BindDescriptorSet(m_sortFarDS, 0);

		for (uint32_t j = k >> 1; j >= waterSimShaders.sortWorkGroupSize; j >>= 1)
		{
			uint32_t jIdx = std::bit_width(j) - std::bit_width(waterSimShaders.sortWorkGroupSize);

			uint32_t indexBufferOffset = m_farSortIndexBufferOffsets[jIdx];
			uint32_t numWorkGroups = m_farSortIndexBufferOffsets[jIdx + 1] - indexBufferOffset;

			cc.Barrier(m_sortedByCellBuffer, computeToComputeBarrier);

			const uint32_t sortFarPushConstants[3] = { k, j, indexBufferOffset };
			cc.PushConstants(0, sizeof(sortFarPushConstants), sortFarPushConstants);
			cc.DispatchCompute(numWorkGroups, 1, 1);
		}

		cc.Barrier(m_sortedByCellBuffer, computeToComputeBarrier);

		cc.BindPipeline(waterSimShaders.sortPipelineNear);
		cc.BindDescriptorSet(m_sortNearDS, 0);
		cc.PushConstants(0, sizeof(uint32_t), &k);
		cc.DispatchCompute(m_sortNumWorkGroups, 1, 1);
	}

	cc.Barrier(m_sortedByCellBuffer, computeToComputeBarrier);
}

void WaterSimulator2::RunCellPreparePhase(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterCellPrepare");

	cc.Barrier(
		m_cellOffsetsTexture,
		{
			.oldUsage = eg::TextureUsage::Undefined,
			.newUsage = eg::TextureUsage::CopyDst,
		}
	);

	cc.ClearColorTexture(m_cellOffsetsTexture, 0, glm::ivec4(0xFFFF));

	cc.Barrier(
		m_cellOffsetsTexture,
		{
			.oldUsage = eg::TextureUsage::CopyDst,
			.newUsage = eg::TextureUsage::ILSWrite,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	// ** DetectCellEdges **
	cc.BindPipeline(waterSimShaders.detectCellEdges);
	uint32_t dataForCPUDynamicOffset = eg::CFrameIdx() * m_particleDataForCPUBufferBytesPerFrame;
	cc.BindDescriptorSet(m_detectCellEdgesDS, 0, { &dataForCPUDynamicOffset, 1 });

	const uint32_t detectCellEdgesPushConstants[] = {
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.x)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.y)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.z)),
		m_numParticles,
		0,                  // posInOffset
		m_numParticles,     // posOutOffset
		m_numParticles * 2, // velInOffset
		m_numParticles * 3, // velOutOffset
	};
	cc.PushConstants(0, sizeof(detectCellEdgesPushConstants), detectCellEdgesPushConstants);

	cc.DispatchCompute(m_numParticles / waterSimShaders.commonWorkGroupSize, 1, 1);

	cc.Barrier(m_cellNumOctGroupsBuffer, computeToComputeBarrier);

	// ** OctGroupsPrefixSum **
	cc.BindPipeline(waterSimShaders.octGroupsPrefixSum);
	cc.BindDescriptorSet(m_octGroupsPrefixSumDS, 0);
	for (uint32_t i = 0; i < m_numOctGroupsPrefixSumLayerOffsets.size(); i++)
	{
		if (m_numOctGroupsPrefixSumLayerOffsets[i] == UINT32_MAX)
			break;

		cc.Barrier(m_cellNumOctGroupsPrefixSumBuffer, computeToComputeBarrier);

		const uint32_t inputLen = m_numOctGroupsPrefixSumLayerLengths[i];

		const uint32_t nextOffset = i + 1 == m_numOctGroupsPrefixSumLayerOffsets.size()
		                                ? UINT32_MAX
		                                : m_numOctGroupsPrefixSumLayerOffsets[i + 1];

		const uint32_t pushConstants[] = {
			m_numOctGroupsPrefixSumLayerOffsets[i],
			inputLen,
			nextOffset,
		};
		cc.PushConstants(0, sizeof(pushConstants), pushConstants);

		uint32_t wgCount = (inputLen + waterSimShaders.commonWorkGroupSize - 1) / waterSimShaders.commonWorkGroupSize;
		cc.DispatchCompute(wgCount, 1, 1);
	}

	cc.Barrier(m_cellNumOctGroupsPrefixSumBuffer, computeToComputeBarrier);

	// ** WriteOctGroups **
	cc.BindPipeline(waterSimShaders.writeOctGroups);
	cc.BindDescriptorSet(m_writeOctGroupsDS, 0);

	const uint32_t writeOctGroupsPushConstants[] = {
		m_numOctGroupsPrefixSumLayerOffsets[0],
		m_numOctGroupsPrefixSumLayerOffsets[1],
		m_numOctGroupsPrefixSumLayerOffsets[2],
		m_numOctGroupsPrefixSumLayerOffsets[3],
		m_numParticles - 1,
	};
	cc.PushConstants(0, sizeof(writeOctGroupsPushConstants), writeOctGroupsPushConstants);

	cc.DispatchCompute(m_numParticles / waterSimShaders.commonWorkGroupSize, 1, 1);

	cc.Barrier(m_particleDataBuffer, computeToComputeBarrier);
	cc.Barrier(m_octGroupRangesBuffer, computeToComputeBarrier);
	cc.Barrier(
		m_totalNumOctGroupsBuffer,
		eg::BufferBarrier{
			.oldUsage = eg::BufferUsage::StorageBufferWrite,
			.newUsage = eg::BufferUsage::IndirectCommandRead,
			.oldAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	cc.Barrier(
		m_cellOffsetsTexture,
		{
			.oldUsage = eg::TextureUsage::ILSWrite,
			.newUsage = eg::TextureUsage::ILSRead,
			.oldAccess = eg::ShaderAccessFlags::Compute,
			.newAccess = eg::ShaderAccessFlags::Compute,
			.subresource = eg::TextureSubresource(),
		}
	);
}

void WaterSimulator2::CalculateDensity(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterDensity");

	cc.BindPipeline(waterSimShaders.calculateDensity);

	cc.BindDescriptorSet(m_forEachNearDS, 0);
	cc.BindDescriptorSet(m_calcDensityDS1, 1);

	const uint32_t pushConstants[] = {
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.x)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.y)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.z)),
		m_numParticles, // position offset
		m_numParticles, // num particles
	};
	cc.PushConstants(0, sizeof(pushConstants), pushConstants);

	eg::gal::DispatchComputeIndirect(cc.Handle(), m_totalNumOctGroupsBuffer.handle, 0);

	cc.Barrier(m_densityBuffer, computeToComputeBarrier);
}

void WaterSimulator2::CalculateAcceleration(eg::CommandContext& cc, float dt)
{
	auto gpuTimer = eg::StartGPUTimer("WaterAccel");

	cc.BindPipeline(waterSimShaders.calculateAcceleration);

	cc.BindDescriptorSet(m_forEachNearDS, 0); // TODO: Probably already bound?
	cc.BindDescriptorSet(m_calcAccelDS1, 1);

	const uint32_t pushConstants[] = {
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.x)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.y)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.z)),
		m_numParticles,     // position offset
		m_numParticles * 3, // velocity offset
		m_numParticles,     // num particles
		std::bit_cast<uint32_t>(dt),
		std::uniform_int_distribution<uint32_t>(0, 0xFF)(m_rng),
	};
	cc.PushConstants(0, sizeof(pushConstants), pushConstants);

	eg::gal::DispatchComputeIndirect(cc.Handle(), m_totalNumOctGroupsBuffer.handle, 0);

	cc.Barrier(m_particleDataBuffer, computeToComputeBarrier);
}

void WaterSimulator2::VelocityDiffusion(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterVelocityDiffusion");

	cc.BindPipeline(waterSimShaders.velocityDiffusion);

	cc.BindDescriptorSet(m_forEachNearDS, 0); // TODO: Probably already bound?

	const uint32_t pushConstants[] = {
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.x)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.y)),
		std::bit_cast<uint32_t>(static_cast<float>(m_voxelMinBounds.z)),
		m_numParticles,     // position offset
		m_numParticles * 3, // velocity offset
		m_numParticles * 2, // velocity out offset
		m_numParticles,     // num particles
	};
	cc.PushConstants(0, sizeof(pushConstants), pushConstants);

	eg::gal::DispatchComputeIndirect(cc.Handle(), m_totalNumOctGroupsBuffer.handle, 0);

	cc.Barrier(m_particleDataBuffer, computeToComputeBarrier);
}

static float* waterGlowTime = eg::TweakVarFloat("water_glow_time", 0.5f);

void WaterSimulator2::MoveAndCollision(eg::CommandContext& cc, float dt)
{
	auto gpuTimer = eg::StartGPUTimer("WaterMoveAndCollision");

	cc.BindPipeline(waterSimShaders.moveAndCollision);

	cc.BindDescriptorSet(m_moveAndCollisionDS, 0);

	const int32_t glowTimeSubtract =
		std::max(1, static_cast<int32_t>(std::round(static_cast<float>(W_MAX_GLOW_TIME) * dt / *waterGlowTime)));

	const uint32_t pushConstants[] = {
		std::bit_cast<uint32_t>(m_voxelMinBounds.x),
		std::bit_cast<uint32_t>(m_voxelMinBounds.y),
		std::bit_cast<uint32_t>(m_voxelMinBounds.z),
		std::bit_cast<uint32_t>(dt),
		std::bit_cast<uint32_t>(m_voxelMaxBounds.x),
		std::bit_cast<uint32_t>(m_voxelMaxBounds.y),
		std::bit_cast<uint32_t>(m_voxelMaxBounds.z),
		m_numParticles,     // position offset
		0,                  // position out offset
		m_numParticles * 2, // velocity offset
		std::bit_cast<uint32_t>(glowTimeSubtract),
	};
	cc.PushConstants(0, sizeof(pushConstants), pushConstants);

	static_assert((WaterSimulator2::NUM_PARTICLES_ALIGNMENT % W_MOVE_AND_COLLISION_WG_SIZE) == 0);

	cc.DispatchCompute(m_numParticles / W_MOVE_AND_COLLISION_WG_SIZE, 1, 1);
}

void WaterSimulator2::Update(eg::CommandContext& cc, float dt, const World& world, bool paused)
{
	if (m_initialFramesCompleted > eg::MAX_CONCURRENT_FRAMES)
	{
		m_particleDataForCPUBuffer.Invalidate(
			eg::CFrameIdx() * m_particleDataForCPUBufferBytesPerFrame, m_particleDataForCPUBufferBytesPerFrame
		);
		std::span<const glm::uvec2> particleData(
			m_particleDataForCPUMappedMemory + eg::CFrameIdx() * m_numParticles, m_numParticles
		);
		m_backgroundThread.OnFrameBeginMT(cc, particleData);
	}
	else
	{
		m_initialFramesCompleted++;
	}

	// Applies gravity changes
	bool hasBoundChangeGravityPipeline = false;
	while (auto gravityChangeResult = m_backgroundThread.PopGravityChangeResult())
	{
		cc.Barrier(
			m_gravityChangeBitsBuffer,
			eg::BufferBarrier{
				.oldUsage = eg::BufferUsage::StorageBufferRead,
				.newUsage = eg::BufferUsage::CopyDst,
				.oldAccess = eg::ShaderAccessFlags::Compute,
			}
		);

		uint32_t numWords = m_numParticles / 32;
		eg::UploadBuffer uploadBuffer =
			eg::GetTemporaryUploadBufferWith<uint32_t>({ gravityChangeResult->changeGravityBits.get(), numWords });

		cc.CopyBuffer(
			uploadBuffer.buffer, m_gravityChangeBitsBuffer, uploadBuffer.offset, 0, numWords * sizeof(uint32_t)
		);

		cc.Barrier(
			m_gravityChangeBitsBuffer,
			eg::BufferBarrier{
				.oldUsage = eg::BufferUsage::CopyDst,
				.newUsage = eg::BufferUsage::StorageBufferRead,
				.newAccess = eg::ShaderAccessFlags::Compute,
			}
		);

		if (!hasBoundChangeGravityPipeline)
		{
			cc.BindPipeline(waterSimShaders.changeGravity);
			cc.BindDescriptorSet(m_gravityChangeDS, 0);
			hasBoundChangeGravityPipeline = true;
		}

		cc.PushConstants(0, static_cast<uint32_t>(gravityChangeResult->newGravity));

		cc.DispatchCompute(m_numParticles / W_CHANGE_GRAVITY_WG_SIZE, 1, 1);

		cc.Barrier(m_particleDataBuffer, computeToComputeBarrier);

		eg::Log(eg::LogLevel::Info, "water", "changed gravity");
	}

	UpdateWaterCollisionQuadsBlockedGravity(world, cc, m_collisionData);

	auto gpuTimer = eg::StartGPUTimer("WaterSim");

	// dt = 1.0f / 60.0f;

	RunSortPhase(cc);
	RunCellPreparePhase(cc);
	CalculateDensity(cc);
	CalculateAcceleration(cc, dt);
	VelocityDiffusion(cc);
	MoveAndCollision(cc, dt);

	cc.Barrier(
		m_particleDataBuffer,
		{
			.oldUsage = eg::BufferUsage::StorageBufferReadWrite,
			.newUsage = eg::BufferUsage::StorageBufferRead,
			.oldAccess = eg::ShaderAccessFlags::Compute,
			.newAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Compute,
		}
	);
}

std::shared_ptr<WaterQueryAABB> WaterSimulator2::AddQueryAABB(const eg::AABB& aabb)
{
	auto query = std::make_shared<WaterQueryAABB>(aabb);
	m_backgroundThread.AddQueryMT(query);
	return query;
}
