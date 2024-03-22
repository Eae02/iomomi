#include "WaterSimulator.hpp"
#include "../Graphics/GraphicsCommon.hpp"
#include "../World/VoxelBuffer.hpp"
#include "WaterConstants.hpp"
#include "WaterGenerate.hpp"
#include "WaterSimulationShaders.hpp"
#include "WaterSimulationThread.hpp"

#include <bit>
#include <cstring>

static_assert((WaterSimulator2::NUM_PARTICLES_ALIGNMENT % W_COMMON_WG_SIZE) == 0);

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

static constexpr eg::BufferFlags STAGING_BUFFER_FLAGS =
	eg::BufferFlags::MapWrite | eg::BufferFlags::CopySrc | eg::BufferFlags::ManualBarrier;

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

static const eg::BufferBarrier barrierCopyToStorageBufferRW = {
	.oldUsage = eg::BufferUsage::CopyDst,
	.newUsage = eg::BufferUsage::StorageBufferReadWrite,
	.newAccess = eg::ShaderAccessFlags::Compute,
};

void WaterSimulator2::SetBufferData(
	eg::CommandContext& cc, eg::BufferRef buffer, uint64_t dataSize, const void* data, bool doBarrier
)
{
	eg::Buffer uploadBuffer = eg::Buffer(STAGING_BUFFER_FLAGS, dataSize, nullptr);
	std::memcpy(uploadBuffer.Map(0, dataSize), data, dataSize);
	uploadBuffer.Flush(0, dataSize);

	cc.CopyBuffer(uploadBuffer, buffer, 0, 0, dataSize);

	if (doBarrier)
		cc.Barrier(buffer, barrierCopyToStorageBufferRW);
}

struct WaterParametersBufferData
{
	glm::ivec3 voxelMinBounds;
	uint32_t numParticles;
	glm::ivec3 voxelMaxBounds;
	uint32_t _padding1;
	float dt;
	int32_t glowTimeSubtract;
	uint32_t randomOffsetsBase;
	uint32_t _padding2;
};

WaterSimulator2::WaterSimulator2(const WaterSimulatorInitArgs& args, eg::CommandContext& cc) : m_rng(time(nullptr))
{
	EG_ASSERT((args.positions.size() % NUM_PARTICLES_ALIGNMENT) == 0);

	eg::BufferFlags downloadFlags = {};
	if (args.enableDataDownload)
		downloadFlags |= eg::BufferFlags::CopySrc;

	auto [minBounds, maxBounds] = args.voxelBuffer->CalculateBounds();

	m_voxelMinBounds = minBounds;
	m_voxelMaxBounds = maxBounds;

	m_backgroundThread = std::make_unique<WaterSimulationThread>(minBounds, args.positions.size());

	m_numParticles = args.positions.size();
	m_numParticlesTwoPow = RoundToNextPowerOf2(std::max(m_numParticles, waterSimShaders.sortWorkGroupSize));

	m_sortNumWorkGroups = m_numParticlesTwoPow / waterSimShaders.sortWorkGroupSize;

	SortFarIndexBuffer sortFarIndexBuffer = ComputeSortFarIndexBuffer(m_numParticlesTwoPow);
	m_farSortIndexBufferOffsets = std::move(sortFarIndexBuffer.firstIndex);

	m_collisionData = WaterCollisionData::Create(cc, *args.voxelBuffer, std::move(args.collisionQuads));

	const uint32_t positionsDataSize = sizeof(float) * 4 * m_numParticles;

	// ** Creates buffers **
	m_positionsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = positionsDataSize,
		.label = "WaterPos",
	});

	m_positionsBufferTemp = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = positionsDataSize,
		.label = "WaterPosTemp",
	});

	m_velocitiesBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = positionsDataSize,
		.label = "WaterVel",
	});

	m_velocitiesBufferTemp = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = positionsDataSize,
		.label = "WaterVelTemp",
	});

	m_particleDataForCPUBufferBytesPerFrame = sizeof(uint32_t) * 2 * m_numParticles;
	for (ParticleDataForCPUBuffer& buffer : m_particleDataForCPUBuffers)
	{
		buffer.buffer = eg::Buffer(eg::BufferCreateInfo{
			.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | eg::BufferFlags::MapRead,
			.size = m_particleDataForCPUBufferBytesPerFrame,
			.label = "ParticleDataDownload",
		});

		buffer.memory = static_cast<const glm::uvec2*>(buffer.buffer.Map());

		buffer.descriptorSet = eg::DescriptorSet(waterSimShaders.detectCellEdges, 1);
		buffer.descriptorSet.BindStorageBuffer(buffer.buffer, 0);
	}

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

	m_constParametersBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier,
		.size = sizeof(WaterParametersBufferData),
		.label = "WaterConstParametersBuffer",
	});

	WaterParametersBufferData parametersBufferData = {
		.voxelMinBounds = minBounds,
		.numParticles = m_numParticles,
		.voxelMaxBounds = maxBounds,
	};
	SetBufferData(cc, m_constParametersBuffer, sizeof(parametersBufferData), &parametersBufferData, false);

	// The cell num oct groups buffer may need to be padded since the W_OCT_GROUPS_PREFIX_SUM_WG_SIZE (256) is larger
	// than the guarenteed alignment of m_numParticles (128)
	const uint32_t numOctGroupsBufferPaddedLen =
		eg::RoundToNextMultiple(m_numParticles, W_OCT_GROUPS_PREFIX_SUM_WG_SIZE);
	m_cellNumOctGroupsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags =
			eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | eg::BufferFlags::CopyDst | downloadFlags,
		.size = sizeof(uint32_t) * numOctGroupsBufferPaddedLen,
	});

	// The padding of the numOctGroups buffer needs to be filled with zeroes because it won't be written to by
	// DetectCellEdges.cs but the prefix sum shaders need this padding to be zeroed.
	if (numOctGroupsBufferPaddedLen != m_numParticles)
	{
		cc.FillBuffer(
			m_cellNumOctGroupsBuffer, m_numParticles * sizeof(uint32_t),
			(numOctGroupsBufferPaddedLen - m_numParticles) * sizeof(uint32_t), 0
		);

		cc.Barrier(m_cellNumOctGroupsBuffer, barrierCopyToStorageBufferRW);
	}

	const uint64_t cellNumOctGroupsPrefixSumBufferSize =
		sizeof(uint32_t) * (W_OCT_GROUPS_PREFIX_SUM_WG_SIZE + numOctGroupsBufferPaddedLen);
	m_cellNumOctGroupsPrefixSumBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = cellNumOctGroupsPrefixSumBufferSize,
	});

	const uint64_t octGroupRangesBufferSize = sizeof(uint32_t) * m_numParticles * W_MAX_OCT_GROUPS_PER_CELL;
	m_octGroupRangesBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags =
			eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = octGroupRangesBufferSize,
	});

	// Writes a dummy value to the octGroupRanges buffer to help the test understand where the written data ends
	if (args.enableDataDownload)
	{
		cc.FillBuffer(m_octGroupRangesBuffer, 0, octGroupRangesBufferSize, 0xcc);
	}

	// This buffer stores the total number of oct groups in the first 4 bytes, followed by two ones in the next 8 bytes
	// so that it can be used for DispatchIndirect.
	m_totalNumOctGroupsBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = eg::BufferFlags::StorageBuffer | eg::BufferFlags::CopyDst | eg::BufferFlags::IndirectCommands |
	             eg::BufferFlags::ManualBarrier | downloadFlags,
		.size = sizeof(uint32_t) * 3,
	});

	// Initialize the total oct groups buffer with 0 1 1
	const uint32_t totalNumOctGroupsInitialData[3] = { 0, 1, 1 };
	SetBufferData(cc, m_totalNumOctGroupsBuffer, sizeof(totalNumOctGroupsInitialData), totalNumOctGroupsInitialData);

	m_randomOffsetsBuffer = CreateRandomOffsetsBuffer(m_rng, cc);

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

	m_cellOffsetsTextureSize = glm::uvec3(glm::ceil(glm::vec3(maxBounds - minBounds + 1) / W_INFLUENCE_RADIUS));
	m_cellOffsetsTexture = eg::Texture::Create3D(eg::TextureCreateInfo{
		.flags = eg::TextureFlags::StorageImage | eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst |
	             eg::TextureFlags::ManualBarrier,
		.mipLevels = 1,
		.width = m_cellOffsetsTextureSize.x,
		.height = m_cellOffsetsTextureSize.y,
		.depth = m_cellOffsetsTextureSize.z,
		.format = eg::Format::R32_UInt,
	});

	// ** Creates the frame parameters buffers **
	m_frameParametersSectionStride = eg::RoundToNextMultiple(
		FRAME_PARAMETERS_BYTES_PER_SECTION, eg::GetGraphicsDeviceInfo().uniformBufferOffsetAlignment
	);
	for (uint32_t i = 0; i < eg::MAX_CONCURRENT_FRAMES; i++)
	{
		m_frameParameterBuffers[i] = eg::Buffer(
			eg::BufferFlags::MapWrite | eg::BufferFlags::MapCoherent | eg::BufferFlags::UniformBuffer,
			FRAME_PARAMETERS_NUM_SECTIONS * m_frameParametersSectionStride, nullptr
		);

		m_frameParameterBufferPointers[i] = m_frameParameterBuffers[i].Map();

		static const eg::DescriptorSetBinding FRAME_PARAMETER_DS_BINDING = {
			.binding = 0,
			.type = eg::BindingTypeUniformBuffer{ .dynamicOffset = true },
			.shaderAccess = eg::ShaderAccessFlags::Compute,
		};

		m_frameParameterDescriptorSets[i] = eg::DescriptorSet({ &FRAME_PARAMETER_DS_BINDING, 1 });
		m_frameParameterDescriptorSets[i].BindUniformBuffer(
			m_frameParameterBuffers[i], 0, eg::BIND_BUFFER_OFFSET_DYNAMIC, FRAME_PARAMETERS_BYTES_PER_SECTION
		);
	}

	// ** Creates descriptor sets **

	m_clearCellOffsetsDS = eg::DescriptorSet(waterSimShaders.clearCellOffsets, 0);
	m_clearCellOffsetsDS.BindStorageImage(m_cellOffsetsTexture, 0);

	m_sortInitialDS = eg::DescriptorSet(waterSimShaders.sortPipelineInitial, 0);
	m_sortInitialDS.BindStorageBuffer(m_positionsBuffer, 0);
	m_sortInitialDS.BindStorageBuffer(m_sortedByCellBuffer, 1);
	m_sortInitialDS.BindUniformBuffer(m_constParametersBuffer, 2);

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
	m_detectCellEdgesDS.BindStorageImage(m_cellOffsetsTexture, 1);
	m_detectCellEdgesDS.BindStorageBuffer(m_cellNumOctGroupsBuffer, 2);
	m_detectCellEdgesDS.BindStorageBuffer(m_positionsBuffer, 3);
	m_detectCellEdgesDS.BindStorageBuffer(m_positionsBufferTemp, 4);
	m_detectCellEdgesDS.BindStorageBuffer(m_velocitiesBuffer, 5);
	m_detectCellEdgesDS.BindStorageBuffer(m_velocitiesBufferTemp, 6);
	m_detectCellEdgesDS.BindUniformBuffer(m_constParametersBuffer, 7);

	m_octGroupsPrefixSumLevel1DS = eg::DescriptorSet(waterSimShaders.octGroupsPrefixSumLevel1, 0);
	m_octGroupsPrefixSumLevel1DS.BindStorageBuffer(m_cellNumOctGroupsBuffer, 0);
	m_octGroupsPrefixSumLevel1DS.BindStorageBuffer(m_cellNumOctGroupsPrefixSumBuffer, 1);

	m_octGroupsPrefixSumLevel2DS = eg::DescriptorSet(waterSimShaders.octGroupsPrefixSumLevel2, 0);
	m_octGroupsPrefixSumLevel2DS.BindStorageBuffer(m_cellNumOctGroupsPrefixSumBuffer, 0, 0, sizeof(uint32_t) * 256);
	m_octGroupsPrefixSumLevel2DS.BindStorageBuffer(m_totalNumOctGroupsBuffer, 1, 0, sizeof(uint32_t));

	m_writeOctGroupsDS = eg::DescriptorSet(waterSimShaders.writeOctGroups, 0);
	m_writeOctGroupsDS.BindStorageBuffer(m_cellNumOctGroupsPrefixSumBuffer, 0);
	m_writeOctGroupsDS.BindStorageBuffer(m_octGroupRangesBuffer, 1);

	static const eg::DescriptorSetBinding ForEachNearDescriptorBindings[] = {
		{
			.binding = 0,
			.type = eg::BindingTypeStorageBuffer{ .rwMode = eg::ReadWriteMode::ReadOnly },
			.shaderAccess = eg::ShaderAccessFlags::Compute,
		},
		{
			.binding = 1,
			.type = eg::BindingTypeStorageBuffer{ .rwMode = eg::ReadWriteMode::ReadWrite },
			.shaderAccess = eg::ShaderAccessFlags::Compute,
		},
		{
			.binding = 2,
			.type =
				eg::BindingTypeTexture{
					.viewType = eg::TextureViewType::Flat3D,
					.sampleMode = eg::TextureSampleMode::UInt,
				},
			.shaderAccess = eg::ShaderAccessFlags::Compute,
		},
		{
			.binding = 3,
			.type = eg::BindingTypeUniformBuffer{},
			.shaderAccess = eg::ShaderAccessFlags::Compute,
		},
	};
	m_forEachNearDS = eg::DescriptorSet(ForEachNearDescriptorBindings);
	m_forEachNearDS.BindStorageBuffer(m_octGroupRangesBuffer, 0);
	m_forEachNearDS.BindStorageBuffer(m_positionsBufferTemp, 1);
	m_forEachNearDS.BindTexture(m_cellOffsetsTexture, 2);
	m_forEachNearDS.BindUniformBuffer(m_constParametersBuffer, 3);

	m_calcDensityDS1 = eg::DescriptorSet(waterSimShaders.calculateDensity, 1);
	m_calcDensityDS1.BindStorageBuffer(m_densityBuffer, 0);

	m_calcAccelDS1 = eg::DescriptorSet(waterSimShaders.calculateAcceleration, 1);
	m_calcAccelDS1.BindStorageBuffer(m_densityBuffer, 0);
	m_calcAccelDS1.BindStorageBuffer(m_velocitiesBufferTemp, 1);
	m_calcAccelDS1.BindUniformBuffer(m_randomOffsetsBuffer, 2);

	m_velocityDiffusionDS1 = eg::DescriptorSet(waterSimShaders.velocityDiffusion, 1);
	m_velocityDiffusionDS1.BindStorageBuffer(m_velocitiesBufferTemp, 0);
	m_velocityDiffusionDS1.BindStorageBuffer(m_velocitiesBuffer, 1);

	m_moveAndCollisionDS = eg::DescriptorSet(waterSimShaders.moveAndCollision, 0);
	m_moveAndCollisionDS.BindStorageBuffer(m_positionsBufferTemp, 0);
	m_moveAndCollisionDS.BindStorageBuffer(m_positionsBuffer, 1);
	m_moveAndCollisionDS.BindStorageBuffer(m_velocitiesBuffer, 2);
	m_moveAndCollisionDS.BindTexture(m_collisionData.voxelDataTexture, 3);
	m_moveAndCollisionDS.BindStorageBuffer(m_collisionData.collisionQuadsBuffer, 4);
	m_moveAndCollisionDS.BindUniformBuffer(m_constParametersBuffer, 5);

	m_gravityChangeDS = eg::DescriptorSet(waterSimShaders.changeGravity, 0);
	m_gravityChangeDS.BindStorageBuffer(m_velocitiesBuffer, 0);
	m_gravityChangeDS.BindStorageBuffer(m_gravityChangeBitsBuffer, 1);

	// ** Uploads data **

	// Uploads positions into the positions buffer and generates particle radii
	eg::Buffer particleDataUploadBuffer = eg::Buffer(eg::BufferCreateInfo{
		.flags = STAGING_BUFFER_FLAGS,
		.size = positionsDataSize * 2,
	});
	float* positionsUploadMemory = static_cast<float*>(particleDataUploadBuffer.Map());
	std::memset(positionsUploadMemory, 0, positionsDataSize * 2);
	uint32_t* dataBitsUploadMemory = static_cast<uint32_t*>(particleDataUploadBuffer.Map()) + m_numParticles * 4 + 3;
	for (size_t i = 0; i < args.positions.size(); i++)
	{
		positionsUploadMemory[i * 4 + 0] = args.positions[i].x;
		positionsUploadMemory[i * 4 + 1] = args.positions[i].y;
		positionsUploadMemory[i * 4 + 2] = args.positions[i].z;

		// Sets the data bits so that the gravity is -Y and the persistent id is the index of the particle
		dataBitsUploadMemory[i * 4] = static_cast<uint32_t>(Dir::NegY) | (static_cast<uint32_t>(i) << 16);
	}
	particleDataUploadBuffer.Flush();
	cc.CopyBuffer(particleDataUploadBuffer, m_positionsBuffer, 0, 0, positionsDataSize);
	cc.CopyBuffer(particleDataUploadBuffer, m_velocitiesBuffer, positionsDataSize, 0, positionsDataSize);

	cc.Barrier(
		m_positionsBuffer,
		eg::BufferBarrier{
			.oldUsage = eg::BufferUsage::CopyDst,
			.newUsage = eg::BufferUsage::StorageBufferRead,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	cc.Barrier(
		m_velocitiesBuffer,
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

void WaterSimulator2::SetFrameParametersBufferData(eg::CommandContext& cc, uint32_t setIndex, glm::uvec4 data)
{
	static_assert(sizeof(data) == FRAME_PARAMETERS_BYTES_PER_SECTION);

	char* basePtr = static_cast<char*>(m_frameParameterBufferPointers[m_frameParameterBufferIndex]);
	uint32_t offset = m_nextFrameParametersSection * m_frameParametersSectionStride;
	std::memcpy(basePtr + offset, &data, sizeof(data));

	m_nextFrameParametersSection++;

	cc.BindDescriptorSet(m_frameParameterDescriptorSets[m_frameParameterBufferIndex], setIndex, { &offset, 1 });
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
	cc.DebugLabelBegin("WaterSort");

	cc.BindPipeline(waterSimShaders.sortPipelineInitial);
	cc.BindDescriptorSet(m_sortInitialDS, 0);
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

			SetFrameParametersBufferData(cc, 1, glm::uvec4(k, j, indexBufferOffset, 0));

			cc.DispatchCompute(numWorkGroups, 1, 1);
		}

		cc.Barrier(m_sortedByCellBuffer, computeToComputeBarrier);

		cc.BindPipeline(waterSimShaders.sortPipelineNear);
		cc.BindDescriptorSet(m_sortNearDS, 0);
		SetFrameParametersBufferData(cc, 1, glm::uvec4(k, 0, 0, 0));
		cc.DispatchCompute(m_sortNumWorkGroups, 1, 1);
	}

	cc.Barrier(m_sortedByCellBuffer, computeToComputeBarrier);

	cc.DebugLabelEnd();
}

void WaterSimulator2::RunCellPreparePhase(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterCellPrepare");
	cc.DebugLabelBegin("WaterCellPrepare");

	cc.Barrier(
		m_cellOffsetsTexture,
		{
			.oldUsage = eg::TextureUsage::Undefined,
			.newUsage = eg::TextureUsage::ILSWrite,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	cc.BindPipeline(waterSimShaders.clearCellOffsets);
	cc.BindDescriptorSet(m_clearCellOffsetsDS, 0);
	glm::uvec3 clearDispatchCount = glm::max((m_cellOffsetsTextureSize + glm::uvec3(3)) / glm::uvec3(4), glm::uvec3(1));
	cc.DispatchCompute(clearDispatchCount.x, clearDispatchCount.y, clearDispatchCount.z);

	cc.Barrier(
		m_cellOffsetsTexture,
		{
			.oldUsage = eg::TextureUsage::ILSWrite,
			.newUsage = eg::TextureUsage::ILSWrite,
			.oldAccess = eg::ShaderAccessFlags::Compute,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	// ** DetectCellEdges **
	cc.DebugLabelBegin("DetectCellEdges");
	cc.BindPipeline(waterSimShaders.detectCellEdges);
	cc.BindDescriptorSet(m_detectCellEdgesDS, 0);
	cc.BindDescriptorSet(m_particleDataForCPUBuffers[m_particleDataForCPUCurrentFrameIndex].descriptorSet, 1);

	cc.DispatchCompute(m_numParticles / W_COMMON_WG_SIZE, 1, 1);

	cc.Barrier(m_cellNumOctGroupsBuffer, computeToComputeBarrier);

	cc.DebugLabelEnd();

	// ** OctGroupsPrefixSum **
	const uint32_t octGroupSectionCount =
		(m_numParticles + W_OCT_GROUPS_PREFIX_SUM_WG_SIZE - 1) / W_OCT_GROUPS_PREFIX_SUM_WG_SIZE;
	cc.DebugLabelBegin("OctGroupsPrefixSumLevel1");
	cc.BindPipeline(waterSimShaders.octGroupsPrefixSumLevel1);
	cc.BindDescriptorSet(m_octGroupsPrefixSumLevel1DS, 0);
	cc.DispatchCompute(octGroupSectionCount, 1, 1);
	cc.Barrier(m_cellNumOctGroupsPrefixSumBuffer, computeToComputeBarrier);
	cc.DebugLabelEnd();

	cc.DebugLabelBegin("OctGroupsPrefixSumLevel2");
	cc.BindPipeline(waterSimShaders.octGroupsPrefixSumLevel2);
	cc.BindDescriptorSet(m_octGroupsPrefixSumLevel2DS, 0);
	SetFrameParametersBufferData(cc, 1, glm::uvec4(octGroupSectionCount, 0, 0, 0));
	cc.DispatchCompute(1, 1, 1);
	cc.Barrier(m_cellNumOctGroupsPrefixSumBuffer, computeToComputeBarrier);
	cc.DebugLabelEnd();

	// ** WriteOctGroups **
	cc.DebugLabelBegin("WriteOctGroups");
	cc.BindPipeline(waterSimShaders.writeOctGroups);
	cc.BindDescriptorSet(m_writeOctGroupsDS, 0);

	cc.DispatchCompute(m_numParticles / W_COMMON_WG_SIZE, 1, 1);

	cc.Barrier(m_positionsBufferTemp, computeToComputeBarrier);
	cc.Barrier(m_velocitiesBufferTemp, computeToComputeBarrier);
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
			.newUsage = eg::TextureUsage::ShaderSample,
			.oldAccess = eg::ShaderAccessFlags::Compute,
			.newAccess = eg::ShaderAccessFlags::Compute,
			.subresource = eg::TextureSubresource(),
		}
	);

	cc.DebugLabelEnd();
	cc.DebugLabelEnd();
}

void WaterSimulator2::CalculateDensity(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterDensity");
	cc.DebugLabelBegin("WaterDensity");

	cc.BindPipeline(waterSimShaders.calculateDensity);

	cc.BindDescriptorSet(m_forEachNearDS, 0);
	cc.BindDescriptorSet(m_calcDensityDS1, 1);

	cc.DispatchComputeIndirect(m_totalNumOctGroupsBuffer, 0);

	cc.Barrier(m_densityBuffer, computeToComputeBarrier);

	cc.DebugLabelEnd();
}

void WaterSimulator2::CalculateAcceleration(eg::CommandContext& cc, float dt)
{
	auto gpuTimer = eg::StartGPUTimer("WaterAccel");
	cc.DebugLabelBegin("WaterAccel");

	cc.BindPipeline(waterSimShaders.calculateAcceleration);

	cc.BindDescriptorSet(m_forEachNearDS, 0); // TODO: Probably already bound?
	cc.BindDescriptorSet(m_calcAccelDS1, 1);

	cc.DispatchComputeIndirect(m_totalNumOctGroupsBuffer, 0);

	cc.Barrier(m_velocitiesBufferTemp, computeToComputeBarrier);

	cc.DebugLabelEnd();
}

void WaterSimulator2::VelocityDiffusion(eg::CommandContext& cc)
{
	auto gpuTimer = eg::StartGPUTimer("WaterVelocityDiffusion");
	cc.DebugLabelBegin("WaterVelocityDiffusion");

	cc.BindPipeline(waterSimShaders.velocityDiffusion);

	cc.BindDescriptorSet(m_forEachNearDS, 0); // TODO: Probably already bound?
	cc.BindDescriptorSet(m_velocityDiffusionDS1, 1);

	cc.DispatchComputeIndirect(m_totalNumOctGroupsBuffer, 0);

	cc.Barrier(m_velocitiesBuffer, computeToComputeBarrier);

	cc.DebugLabelEnd();
}

static float* waterGlowTime = eg::TweakVarFloat("water_glow_time", 0.5f);

void WaterSimulator2::MoveAndCollision(eg::CommandContext& cc, float dt)
{
	auto gpuTimer = eg::StartGPUTimer("WaterMoveAndCollision");
	cc.DebugLabelBegin("WaterMoveAndCollision");

	cc.BindPipeline(waterSimShaders.moveAndCollision);

	cc.BindDescriptorSet(m_moveAndCollisionDS, 0);

	cc.DispatchCompute(m_numParticles / W_COMMON_WG_SIZE, 1, 1);

	cc.DebugLabelEnd();
}

void WaterSimulator2::Update(eg::CommandContext& cc, float dt, const World& world)
{
	eg::MultiStageCPUTimer cpuTimer;

	m_nextFrameParametersSection = 0;

	cpuTimer.StartStage("Copy for BT");

	if (m_initialFramesCompleted > m_particleDataForCPUBuffers.size())
	{
		m_particleDataForCPUBuffers[m_particleDataForCPUCurrentFrameIndex].buffer.Invalidate(
			eg::CFrameIdx() * m_particleDataForCPUBufferBytesPerFrame, m_particleDataForCPUBufferBytesPerFrame
		);
		std::span<const glm::uvec2> particleData(
			m_particleDataForCPUBuffers[m_particleDataForCPUCurrentFrameIndex].memory, m_numParticles
		);
		m_backgroundThread->OnFrameBeginMT(particleData);
	}
	else
	{
		m_initialFramesCompleted++;
	}

	m_particleDataForCPUCurrentFrameIndex =
		(m_particleDataForCPUCurrentFrameIndex + 1) % m_particleDataForCPUBuffers.size();

	cpuTimer.StartStage("Gravity Change");

	// Applies gravity changes
	bool hasBoundChangeGravityPipeline = false;
	while (auto gravityChangeResult = m_backgroundThread->PopGravityChangeResult())
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

		SetFrameParametersBufferData(
			cc, 1, glm::uvec4(static_cast<uint32_t>(gravityChangeResult->newGravity), 0, 0, 0)
		);

		cc.DispatchCompute(m_numParticles / W_COMMON_WG_SIZE, 1, 1);

		cc.Barrier(m_velocitiesBuffer, computeToComputeBarrier);
	}

	cpuTimer.StartStage("UpdateWaterCollisionQuadsBlockedGravity");

	UpdateWaterCollisionQuadsBlockedGravity(world, cc, m_collisionData);

	auto gpuTimer = eg::StartGPUTimer("WaterSim");

	dt = std::clamp(dt, 1.0f / 120.0f, 1.0f / 60.0f);

	const int32_t glowTimeSubtract =
		std::max(1, static_cast<int32_t>(std::round(static_cast<float>(W_MAX_GLOW_TIME) * dt / *waterGlowTime)));

	WaterParametersBufferData parametersBufferDataPatch = {
		.dt = dt,
		.glowTimeSubtract = glowTimeSubtract,
		.randomOffsetsBase = std::uniform_int_distribution<uint32_t>(0, 0xFF)(m_rng),
	};
	m_constParametersBuffer.DCUpdateData(offsetof(WaterParametersBufferData, dt), 16, &parametersBufferDataPatch.dt);
	cc.Barrier(
		m_constParametersBuffer,
		eg::BufferBarrier{
			.oldUsage = eg::BufferUsage::CopyDst,
			.newUsage = eg::BufferUsage::UniformBuffer,
			.newAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	cpuTimer.StartStage("CommandsRecord");

	RunSortPhase(cc);
	RunCellPreparePhase(cc);
	CalculateDensity(cc);
	CalculateAcceleration(cc, dt);
	VelocityDiffusion(cc);
	MoveAndCollision(cc, dt);

	cc.Barrier(
		m_positionsBuffer,
		{
			.oldUsage = eg::BufferUsage::StorageBufferReadWrite,
			.newUsage = eg::BufferUsage::StorageBufferRead,
			.oldAccess = eg::ShaderAccessFlags::Compute,
			.newAccess = eg::ShaderAccessFlags::Vertex | eg::ShaderAccessFlags::Compute,
		}
	);

	cc.Barrier(
		m_particleDataForCPUBuffers[m_particleDataForCPUCurrentFrameIndex].buffer,
		{
			.oldUsage = eg::BufferUsage::StorageBufferWrite,
			.newUsage = eg::BufferUsage::HostRead,
			.oldAccess = eg::ShaderAccessFlags::Compute,
		}
	);

	if (m_nextFrameParametersSection != 0 &&
	    !eg::HasFlag(eg::GetGraphicsDeviceInfo().features, eg::DeviceFeatureFlags::MapCoherent))
	{
		m_frameParameterBuffers[m_frameParameterBufferIndex].Flush(
			0, m_frameParametersSectionStride * m_nextFrameParametersSection
		);
	}
	m_frameParameterBufferIndex = (m_frameParameterBufferIndex + 1) % m_frameParameterBuffers.size();
}

std::shared_ptr<WaterQueryAABB> WaterSimulator2::AddQueryAABB(const eg::AABB& aabb)
{
	auto query = std::make_shared<WaterQueryAABB>(aabb);
	m_backgroundThread->AddQueryMT(query);
	return query;
}
