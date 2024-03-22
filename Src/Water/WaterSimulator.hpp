#pragma once

#include "../World/Dir.hpp"
#include <pcg_random.hpp>

#include "WaterCollisionData.hpp"
#include "WaterQuery.hpp"
#include "WaterSimulationThread.hpp"

struct WaterSimulatorInitArgs
{
	std::span<const glm::vec3> positions;
	const class VoxelBuffer* voxelBuffer;

	std::vector<WaterCollisionQuad> collisionQuads;

	bool enableDataDownload;
};

class WaterSimulator2
{
public:
	friend class WaterSimulatorTester;

	WaterSimulator2(const WaterSimulatorInitArgs& args, eg::CommandContext& cc);

	void Update(eg::CommandContext& cc, float dt, const class World& world);

	void EnqueueGravityChange(const eg::Ray& ray, float maxDistance, Dir newGravity)
	{
		m_backgroundThread->PushGravityChangeMT({ .ray = ray, .maxDistance = maxDistance, .newGravity = newGravity });
	}

	eg::BufferRef GetPositionsGPUBuffer() const { return m_positionsBuffer; }

	uint32_t NumParticles() const { return m_numParticles; }

	std::shared_ptr<WaterQueryAABB> AddQueryAABB(const eg::AABB& aabb);

	static constexpr uint32_t NUM_PARTICLES_ALIGNMENT = 128; // the number of particles must be divisible by this

	static void SetBufferData(
		eg::CommandContext& cc, eg::BufferRef buffer, uint64_t dataSize, const void* data, bool doBarrier = true
	);

private:
	void RunSortPhase(eg::CommandContext& cc);

	void RunCellPreparePhase(eg::CommandContext& cc);

	void CalculateDensity(eg::CommandContext& cc);
	void CalculateAcceleration(eg::CommandContext& cc, float dt);
	void VelocityDiffusion(eg::CommandContext& cc);
	void MoveAndCollision(eg::CommandContext& cc, float dt);

	void SetFrameParametersBufferData(eg::CommandContext& cc, uint32_t setIndex, glm::uvec4 data);

	pcg32_fast m_rng;

	uint32_t m_numParticles;
	uint32_t m_numParticlesTwoPow; // m_numParticles rounded up to the next power of 2

	glm::ivec3 m_voxelMinBounds;
	glm::ivec3 m_voxelMaxBounds;

	glm::uvec3 m_cellOffsetsTextureSize;

	std::unique_ptr<WaterSimulationThread> m_backgroundThread;
	uint32_t m_initialFramesCompleted = 0;

	uint32_t m_sortNumWorkGroups;

	WaterCollisionData m_collisionData;

	eg::Texture m_cellOffsetsTexture;

	eg::Buffer m_cellNumOctGroupsBuffer;
	eg::Buffer m_cellNumOctGroupsPrefixSumBuffer;

	eg::Buffer m_totalNumOctGroupsBuffer;
	eg::Buffer m_octGroupRangesBuffer;

	eg::Buffer m_constParametersBuffer;

	static constexpr uint32_t FRAME_PARAMETERS_BYTES_PER_SECTION = 16;
	static constexpr uint32_t FRAME_PARAMETERS_NUM_SECTIONS = 512;

	std::array<eg::Buffer, eg::MAX_CONCURRENT_FRAMES> m_frameParameterBuffers;
	std::array<void*, eg::MAX_CONCURRENT_FRAMES> m_frameParameterBufferPointers;
	std::array<eg::DescriptorSet, eg::MAX_CONCURRENT_FRAMES> m_frameParameterDescriptorSets;
	uint32_t m_frameParametersSectionStride;
	uint32_t m_frameParameterBufferIndex = 0;
	uint32_t m_nextFrameParametersSection = 0;

	uint64_t m_particleDataForCPUCurrentFrameIndex = 0;
	uint32_t m_particleDataForCPUBufferBytesPerFrame;
	struct ParticleDataForCPUBuffer
	{
		eg::Buffer buffer;
		const glm::uvec2* memory;
		eg::DescriptorSet descriptorSet;
	};
	std::array<ParticleDataForCPUBuffer, eg::MAX_CONCURRENT_FRAMES + 1> m_particleDataForCPUBuffers;

	eg::Buffer m_gravityChangeBitsBuffer;

	eg::Buffer m_positionsBuffer;
	eg::Buffer m_positionsBufferTemp;
	eg::Buffer m_velocitiesBuffer;
	eg::Buffer m_velocitiesBufferTemp;

	eg::Buffer m_randomOffsetsBuffer;

	eg::Buffer m_farSortIndexBuffer;
	std::vector<uint32_t> m_farSortIndexBufferOffsets;
	eg::Buffer m_sortedByCellBuffer;

	eg::Buffer m_densityBuffer;

	eg::DescriptorSet m_clearCellOffsetsDS;

	eg::DescriptorSet m_sortInitialDS;
	eg::DescriptorSet m_sortNearDS;
	eg::DescriptorSet m_sortFarDS;

	eg::DescriptorSet m_detectCellEdgesDS;
	eg::DescriptorSet m_octGroupsPrefixSumLevel1DS;
	eg::DescriptorSet m_octGroupsPrefixSumLevel2DS;
	eg::DescriptorSet m_writeOctGroupsDS;

	eg::DescriptorSet m_forEachNearDS;

	eg::DescriptorSet m_calcDensityDS1;
	eg::DescriptorSet m_calcAccelDS1;
	eg::DescriptorSet m_velocityDiffusionDS1;

	eg::DescriptorSet m_moveAndCollisionDS;

	eg::DescriptorSet m_gravityChangeDS;
};
