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

static std::vector<glm::vec3> GeneratePositions(pcg32_fast& rng, uint32_t numParticles)
{
	std::uniform_real_distribution<float> positionDistribution(0.0, 1.0);

	std::uniform_int_distribution<int> cellDistribution(MIN_AIR_VOXEL, MAX_AIR_VOXEL);

	std::vector<glm::ivec3> cells(numParticles / 100);
	for (glm::ivec3& cell : cells)
	{
		cell.x = cellDistribution(rng);
		cell.y = cellDistribution(rng);
		cell.z = cellDistribution(rng);
	}

	std::vector<glm::vec3> positions(numParticles);
	for (glm::vec3& pos : positions)
	{
		size_t cellIndex = std::uniform_int_distribution<size_t>(0, cells.size() - 1)(rng);

		pos.x = static_cast<float>(cells[cellIndex].x) + positionDistribution(rng);
		pos.y = static_cast<float>(cells[cellIndex].y) + positionDistribution(rng);
		pos.z = static_cast<float>(cells[cellIndex].z) + positionDistribution(rng);
	}

	return positions;
}

std::vector<glm::vec2> CalculateDensities(std::span<const glm::vec3> positions)
{
	std::vector<glm::vec2> densities(positions.size());

	for (uint32_t a = 0; a < positions.size(); a++)
	{
		glm::vec2 density(1.0f);

		for (uint32_t b = 0; b < positions.size(); b++)
		{
			if (a == b)
				continue;

			float q = std::max(1.0f - glm::distance(positions[a], positions[b]) / W_INFLUENCE_RADIUS, 0.0f);
			float q2 = q * q;
			float q3 = q * q2;
			float q4 = q2 * q2;
			density += glm::vec2(q3, q4);
		}

		densities[a] = density;
	}

	return densities;
}

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
		std::vector<glm::vec3> positions = GeneratePositions(rng, numParticles);

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

	static void RunSingleSortTest(uint32_t numParticles, uint32_t seed);
};

void WaterSimulatorTests::RunSortTests()
{
	WaterSimulatorTester::RunSingleSortTest(25000, 3);

	// for (uint32_t size : { 1024, 1500, 2048, 2049, 2500, 10000 })
	// {
	// 	for (uint32_t seed = 0; seed < 5; seed++)
	// 	{
	// 		WaterSimulatorTester::RunSingleSortTest(size, seed);
	// 	}
	// }
}

void WaterSimulatorTester::RunSingleSortTest(uint32_t numParticles, uint32_t seed)
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
	tester.simulator->RunCellPreparePhase(tester.cc);
	tester.simulator->CalculateDensity(tester.cc);
	tester.simulator->CalculateAcceleration(tester.cc, 1.0f / 60.0f);
	tester.simulator->VelocityDiffusion(tester.cc);
	tester.simulator->MoveAndCollision(tester.cc, 1.0f / 60.0f);

	tester.BarrierForDownload(tester.simulator->m_sortedByCellBuffer);
	sortedByCellDownbuf.CopyData(tester.cc, tester.simulator->m_sortedByCellBuffer);

	tester.BarrierForDownload(tester.simulator->m_octGroupRangesBuffer);
	octGroupRangesDownbuf.CopyData(tester.cc, tester.simulator->m_octGroupRangesBuffer);

	tester.BarrierForDownload(tester.simulator->m_totalNumOctGroupsBuffer);
	totalNumOctGroupsDownbuf.CopyData(tester.cc, tester.simulator->m_totalNumOctGroupsBuffer);

	tester.BarrierForDownload(tester.simulator->m_densityBuffer);
	densitiesDownloadBuffer.CopyData(tester.cc, tester.simulator->m_densityBuffer);

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

			// std::cout << "L: " << rangeStartCellID << " " << (sortedByCellIdx - octGroupRanges[i]) << std::endl;
		}
	}

	std::span<glm::vec2> densitiesFromGpu = densitiesDownloadBuffer.GetData<glm::vec2>();
	std::vector<glm::vec2> expectedDensities = CalculateDensities(tester.positions);
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
