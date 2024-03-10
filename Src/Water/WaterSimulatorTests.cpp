#include "WaterSimulatorTests.hpp"
#include "../World/VoxelBuffer.hpp"
#include "WaterConstants.hpp"
#include "WaterSimulator.hpp"

#include <chrono>
#include <pcg_random.hpp>
#include <random>
#include <unordered_set>

constexpr int MIN_AIR_VOXEL = -10;
constexpr int MAX_AIR_VOXEL = 10;

std::vector<glm::vec3> WaterTestGeneratePositions(pcg32_fast& rng, uint32_t numParticles, int minAirVoxel, int maxAirVoxel);
std::vector<glm::vec2> WaterTestCalculateDensities(std::span<const glm::vec3> positions);

constexpr eg::BufferFlags DOWNLOAD_BUFFER_FLAGS = eg::BufferFlags::MapRead | eg::BufferFlags::CopyDst |
                                                  eg::BufferFlags::Download | eg::BufferFlags::HostAllocate |
                                                  eg::BufferFlags::ManualBarrier;

struct DownloadBuffer
{
	uint64_t size;
	eg::Buffer buffer;

	explicit DownloadBuffer(uint64_t _size) : size(_size), buffer(DOWNLOAD_BUFFER_FLAGS, _size, nullptr) {}

	void CopyData(eg::CommandContext& cc, eg::BufferRef from) { CopyData(cc, from, 0, size); }

	void CopyData(eg::CommandContext& cc, eg::BufferRef from, uint64_t fromOffset, uint64_t copySize)
	{
		cc.CopyBuffer(from, buffer, fromOffset, 0, copySize);
		cc.Barrier(
			buffer, eg::BufferBarrier{ .oldUsage = eg::BufferUsage::CopyDst, .newUsage = eg::BufferUsage::HostRead }
		);
	}

	template <typename T>
	std::span<T> GetData()
	{
		buffer.Invalidate();
		return std::span<T>(static_cast<T*>(buffer.Map()), size / sizeof(T));
	}
};

struct WaterSimulatorTester
{
	pcg32_fast rng;
	eg::CommandContext cc;
	std::unique_ptr<WaterSimulator2> simulator;

	std::vector<glm::vec3> positions;

	static WaterSimulatorTester Create(uint32_t numParticles, uint32_t seed)
	{
		pcg32_fast rng(seed);
		std::vector<glm::vec3> positions = WaterTestGeneratePositions(rng, numParticles, MIN_AIR_VOXEL, MAX_AIR_VOXEL);

		eg::CommandContext cc = eg::CommandContext::CreateDeferred(eg::Queue::Main);

		cc.BeginRecording(eg::CommandContextBeginFlags::OneTimeSubmit);

		VoxelBuffer voxelBuffer;
		for (int x = MIN_AIR_VOXEL; x <= MAX_AIR_VOXEL; x++)
			for (int y = MIN_AIR_VOXEL; y <= MAX_AIR_VOXEL; y++)
				for (int z = MIN_AIR_VOXEL; z <= MAX_AIR_VOXEL; z++)
					voxelBuffer.SetIsAir(glm::ivec3(x, y, z), true);

		auto simulator = std::make_unique<WaterSimulator2>(
			WaterSimulatorInitArgs{
				.positions = positions,
				.voxelBuffer = &voxelBuffer,
				.enableDataDownload = true,
			},
			cc
		);

		return WaterSimulatorTester{
			.rng = std::move(rng),
			.cc = std::move(cc),
			.simulator = std::move(simulator),
			.positions = std::move(positions),
		};
	}

	void BarrierForDownload(eg::BufferRef buffer)
	{
		cc.Barrier(
			buffer,
			eg::BufferBarrier{
				.oldUsage = eg::BufferUsage::StorageBufferReadWrite,
				.newUsage = eg::BufferUsage::CopySrc,
				.oldAccess = eg::ShaderAccessFlags::Compute,
			}
		);
	}

	void FinishSubmitAndWait()
	{
		cc.FinishRecording();

		eg::FenceHandle fence = eg::gal::CreateFence();
		cc.Submit(eg::CommandContextSubmitArgs{ .fence = fence });

		auto startTime = std::chrono::high_resolution_clock::now();
		eg::gal::WaitForFence(fence, UINT64_MAX);
		auto endTime = std::chrono::high_resolution_clock::now();
		eg::gal::DestroyFence(fence);

		std::cout << "cpu wait time: " << ((endTime - startTime).count() / 1E6) << "ms" << std::endl;
	}

	static void RunSingleSortTest(uint32_t numParticles, uint32_t seed, int finalPhase);
};

enum
{
	TEST_PHASE_SORT,
	TEST_PHASE_CELL_PREPARE,
	TEST_PHASE_DENSITY,
	TEST_PHASE_ACCEL,
	TEST_PHASE_VELOCITY_DIFFUSION,
	TEST_PHASE_MOVE_AND_COLLISION,
};

void RunWaterTests(std::string_view cmdLine)
{
	std::vector<std::string_view> parts;
	eg::SplitString(cmdLine, ':', parts);

	int until = TEST_PHASE_MOVE_AND_COLLISION;
	int numParticles = 1000;
	int numIterations = 1;
	if (parts.size() >= 2)
		numParticles = std::stoi(std::string(parts[1]));
	if (parts.size() >= 3)
	{
		if (parts[2] == "sort")
			until = TEST_PHASE_SORT;
		if (parts[2] == "prep")
			until = TEST_PHASE_CELL_PREPARE;
		if (parts[2] == "density")
			until = TEST_PHASE_DENSITY;
	}
	if (parts.size() >= 4)
		numIterations = std::stoi(std::string(parts[3]));

	for (int i = 0; i < numIterations; i++)
		WaterSimulatorTester::RunSingleSortTest(numParticles, i, until);
}

void WaterSimulatorTester::RunSingleSortTest(uint32_t numParticles, uint32_t seed, int finalPhase)
{
	std::cout << "RunSingleSortTest(" << numParticles << ", " << seed << ")" << std::endl;

	numParticles = eg::RoundToNextMultiple(numParticles, WaterSimulator2::NUM_PARTICLES_ALIGNMENT);

	WaterSimulatorTester tester = WaterSimulatorTester::Create(numParticles, seed);

	DownloadBuffer sortedByCellDownbuf(sizeof(uint32_t) * 2 * tester.simulator->m_numParticlesTwoPow);
	DownloadBuffer octGroupRangesDownbuf(
		sizeof(uint32_t) * tester.simulator->m_numParticles * W_MAX_OCT_GROUPS_PER_CELL
	);
	DownloadBuffer totalNumOctGroupsDownbuf(12);
	DownloadBuffer densitiesDownloadBuffer(tester.simulator->m_numParticles * sizeof(glm::vec2));

	tester.simulator->RunSortPhase(tester.cc);
	if (finalPhase >= TEST_PHASE_CELL_PREPARE)
		tester.simulator->RunCellPreparePhase(tester.cc);
	if (finalPhase >= TEST_PHASE_DENSITY)
		tester.simulator->CalculateDensity(tester.cc);
	if (finalPhase >= TEST_PHASE_ACCEL)
		tester.simulator->CalculateAcceleration(tester.cc, 1.0f / 60.0f);
	if (finalPhase >= TEST_PHASE_VELOCITY_DIFFUSION)
		tester.simulator->VelocityDiffusion(tester.cc);
	if (finalPhase >= TEST_PHASE_MOVE_AND_COLLISION)
		tester.simulator->MoveAndCollision(tester.cc, 1.0f / 60.0f);

	tester.BarrierForDownload(tester.simulator->m_sortedByCellBuffer);
	sortedByCellDownbuf.CopyData(tester.cc, tester.simulator->m_sortedByCellBuffer);

	if (finalPhase >= TEST_PHASE_CELL_PREPARE)
	{
		tester.BarrierForDownload(tester.simulator->m_octGroupRangesBuffer);
		octGroupRangesDownbuf.CopyData(tester.cc, tester.simulator->m_octGroupRangesBuffer);

		tester.BarrierForDownload(tester.simulator->m_totalNumOctGroupsBuffer);
		totalNumOctGroupsDownbuf.CopyData(tester.cc, tester.simulator->m_totalNumOctGroupsBuffer);
	}

	if (finalPhase >= TEST_PHASE_DENSITY)
	{
		tester.BarrierForDownload(tester.simulator->m_densityBuffer);
		densitiesDownloadBuffer.CopyData(tester.cc, tester.simulator->m_densityBuffer);
	}

	tester.FinishSubmitAndWait();

	std::span<glm::uvec2> sortedByCell = sortedByCellDownbuf.GetData<glm::uvec2>();

	bool isSorted = true;
	for (uint32_t i = 1; i < numParticles; i++)
	{
		if (sortedByCell[i].x < sortedByCell[i - 1].x)
			isSorted = false;
	}

	if (!isSorted)
		throw std::runtime_error("data is not sorted");

	if (finalPhase < TEST_PHASE_CELL_PREPARE)
		return;

	std::unordered_set<uint32_t> activeCellsSet;
	for (uint32_t i = 0; i < numParticles; i++)
		activeCellsSet.insert(sortedByCell[i].x);

	std::span<uint32_t> totalNumOctGroups = totalNumOctGroupsDownbuf.GetData<uint32_t>();
	std::cout << "TotOctGroups: " << totalNumOctGroups[0] << " " << totalNumOctGroups[1] << " " << totalNumOctGroups[2]
			  << "\n";

	std::span<uint32_t> octGroupRanges = octGroupRangesDownbuf.GetData<uint32_t>();

	uint32_t sortedByCellIdx = 0;
	for (uint32_t i = 0; i < octGroupRanges.size(); i++)
	{
		bool isDummyValue = octGroupRanges[i] == 0xcccccccc;
		if (isDummyValue != (i >= totalNumOctGroups[0]))
			throw std::runtime_error("invalid octgroup ranges, unexpected value at index " + std::to_string(i));

		if (!isDummyValue)
		{
			if (octGroupRanges[i] != sortedByCellIdx)
				throw std::runtime_error("invalid octgroup ranges, offsets mismatch");

			const uint32_t rangeStartCellID = sortedByCell[sortedByCellIdx].x;
			for (uint32_t j = 0; j < 8; j++)
			{
				if (sortedByCellIdx >= sortedByCell.size() || sortedByCell[sortedByCellIdx].x != rangeStartCellID)
					break;
				sortedByCellIdx++;
			}
		}
	}

	if (finalPhase < TEST_PHASE_DENSITY)
		return;

	std::span<glm::vec2> densitiesFromGpu = densitiesDownloadBuffer.GetData<glm::vec2>();
	std::vector<glm::vec2> expectedDensities = WaterTestCalculateDensities(tester.positions);
	if (densitiesFromGpu.size() != expectedDensities.size())
		throw std::runtime_error("mismatched density count");

	constexpr float ERROR_TOLERANCE = 1E-5f;
	for (uint32_t i = 0; i < numParticles; i++)
	{
		uint32_t originalI = sortedByCell[i].y;
		glm::vec2 expected = expectedDensities[originalI];
		glm::vec2 calculated = densitiesFromGpu[i];

		// std::cout << densitiesFromGpu[i].x << " " << densitiesFromGpu[i].y << "   "
		// 		  << expectedDensities[originalI].x << " " << expectedDensities[originalI].y << std::endl;

		if (glm::distance(expected, calculated) > ERROR_TOLERANCE)
			throw std::runtime_error("density error " + std::to_string(glm::distance(expected, calculated)));
	}
}
