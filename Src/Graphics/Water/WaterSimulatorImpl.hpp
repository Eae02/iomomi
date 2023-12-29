#pragma once

#include "../../World/Dir.hpp"
#include "WaterPumpDescription.hpp"
#include "WaterQueryResults.hpp"
#include "WaterSimulationConstants.hpp"

#include <barrier>
#include <span>
#include <thread>

#include <pcg_random.hpp>

struct WaterBlocker
{
	glm::vec3 center;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 biTangent;
	float tangentLen;
	float biTangentLen;
	uint8_t blockedGravities;
};

class WaterSimulatorImpl
{
public:
	struct ConstructorArgs
	{
		glm::ivec3 minBounds;
		glm::ivec3 maxBounds;
		uint8_t* isAirBuffer;
		uint32_t extraParticles;
		std::span<const glm::vec3> particlePositions;
	};

	struct SimulateArgs
	{
		float dt;
		float gameTime;

		glm::vec3 cameraPos;

		bool shouldChangeParticleGravity;
		bool changeGravityParticleHighlightOnly;
		glm::vec3 changeGravityParticlePos;
		Dir newGravity;

		std::span<const WaterBlocker> waterBlockers;
		std::span<const WaterPumpDescription> waterPumps;
	};

	WaterSimulatorImpl(const WaterSimulatorImpl&) = delete;
	WaterSimulatorImpl(WaterSimulatorImpl&&) = delete;
	WaterSimulatorImpl& operator=(const WaterSimulatorImpl&) = delete;
	WaterSimulatorImpl& operator=(WaterSimulatorImpl&&) = delete;

	static std::unique_ptr<WaterSimulatorImpl> CreateInstance(const ConstructorArgs& args);

	WaterSimulatorImpl(const ConstructorArgs& args, size_t memoryAlignment);

	virtual ~WaterSimulatorImpl();

	uint32_t CopyToOutputBuffer();
	void* GetOutputBuffer() const;

	void* GetGravitiesOutputBuffer(uint32_t& versionOut) const;

	virtual WaterQueryResults Query(const eg::AABB& aabb) const;

	void SwapBuffers();

	void Simulate(const SimulateArgs& simulateArgs);

protected:
	void MoveAcrossPumps(std::span<const WaterPumpDescription> pumps, float dt);
	void ChangeParticleGravity(glm::vec3 changePos, bool highlightOnly, Dir newGravity, float gameTime);

	int CellIdx(glm::ivec3 coord) const;
	bool IsVoxelAir(glm::ivec3 coord) const;

	struct SimulateStageArgs
	{
		uint32_t threadIndex;
		uint32_t loIdx;
		uint32_t hiIdx;
		float dt;
	};

	virtual void Stage1_DetectClose(SimulateStageArgs args);
	virtual void Stage2_ComputeNumberDensity(SimulateStageArgs args);
	virtual void Stage3_Acceleration(SimulateStageArgs args);
	virtual void Stage4_DiffusionAndCollision(SimulateStageArgs args, std::span<const WaterBlocker> waterBlockers);

	uint32_t m_numParticles;
	uint32_t m_allocatedParticles;

	struct Vec3SOA
	{
		float* x;
		float* y;
		float* z;

		glm::vec3 operator[](uint32_t idx) const { return glm::vec3(x[idx], y[idx], z[idx]); }
	};

	Vec3SOA m_particlesPos1;
	Vec3SOA m_particlesVel1;
	Vec3SOA m_particlesPos2;
	Vec3SOA m_particlesVel2;

	uint8_t* m_particlesGravity;
	uint8_t* m_particlesGravity2;
	float* m_particlesGlowTime;

	float* m_particleDensityX;
	float* m_particleDensityY;

	float* m_particlesRadius;

	std::vector<pcg32_fast> m_threadRngs;

	glm::vec4* m_outputBuffer;

	uint32_t m_itemsPerThreadPreferredDivisibility;

	// Stores which cell each particle belongs to
	std::vector<glm::ivec3> particleCells;

	// For each partition cell, stores how many particles belong to that cell.
	std::vector<uint16_t> cellNumParticles;

	// For each partition cell, stores a list of particle indices that belong to that cell.
	std::vector<std::array<uint16_t, MAX_PER_PART_CELL>> cellParticles;

	glm::ivec3 partGridMin;
	glm::ivec3 partGridNumCellGroups;
	glm::ivec3 partGridNumCells;

	uint32_t GetCloseParticlesCount(uint32_t particle) const { return m_numCloseParticles[particle]; }

	float* GetCloseParticlesDistPtr(uint32_t particle) const { return m_closeParticlesDist + particle * MAX_CLOSE; }

	uint16_t* GetCloseParticlesIdxPtr(uint32_t particle) const { return m_closeParticlesIdx + particle * MAX_CLOSE; }

	static const glm::vec3 gravities[6];

private:
	std::pair<uint32_t, uint32_t> GetThreadWorkingRange(uint32_t threadIndex) const;

	void RunAllParallelizedSimulationStages(
		uint32_t threadIndex, float dt, std::span<const WaterBlocker> waterBlockers);

	void WorkerThreadTarget(uint32_t threadIndex);

	std::unique_ptr<void, eg::FreeDel> m_dataMemory;

	uint16_t* m_numCloseParticles;
	uint16_t* m_closeParticlesIdx;
	float* m_closeParticlesDist;

	uint32_t gravityVersion = 0;
	uint32_t gravityVersion2 = 0;

	glm::ivec3 worldMin;
	glm::ivec3 worldSize;

	int voxelAirStrideZ;
	uint8_t* isVoxelAir;

	uint64_t m_workerThreadRunIteration = 0;
	float m_dtForWorkerThread;
	std::span<const WaterBlocker> m_waterBlockersForWorkerThread;
	std::mutex m_workerThreadWakeLock;
	std::condition_variable m_workerThreadWakeSignal;

	uint32_t m_numThreads;

	std::barrier<> m_barrier;

	std::vector<std::thread> m_workerThreads;
};
