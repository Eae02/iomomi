#include "WaterSimulatorImpl.hpp"
#include "WaterSimulationConstants.hpp"

#include <SDL2/SDL_cpuinfo.h>

std::unique_ptr<WaterSimulatorImpl> CreateWaterSimulatorImplAvx2(const WaterSimulatorImpl::ConstructorArgs& args);

std::unique_ptr<WaterSimulatorImpl> WaterSimulatorImpl::CreateInstance(const ConstructorArgs& args)
{
	if (SDL_HasAVX2())
	{
		return CreateWaterSimulatorImplAvx2(args);
	}
	return std::make_unique<WaterSimulatorImpl>(args, alignof(float));
}

static std::uniform_real_distribution<float> radiusDist(MIN_PARTICLE_RADIUS, MAX_PARTICLE_RADIUS);

static int GetThreadCount()
{
	constexpr int THREAD_COUNT_ADD = 8;
	return std::max(static_cast<int>(std::thread::hardware_concurrency()) + THREAD_COUNT_ADD, 1);
}

WaterSimulatorImpl::WaterSimulatorImpl(const ConstructorArgs& args, size_t memoryAlignment)
	: m_numThreads(GetThreadCount()), m_barrier(m_numThreads)
{
	pcg32_fast initialRng(time(nullptr));
	for (uint32_t i = 0; i < m_numThreads; i++)
	{
		m_threadRngs.emplace_back(initialRng());
	}

	m_itemsPerThreadPreferredDivisibility = std::hardware_destructive_interference_size / 4;

	m_numParticles = args.particlePositions.size();
	m_allocatedParticles = m_numParticles + args.extraParticles;

	uint32_t allocatedParticlesAlign = memoryAlignment / sizeof(float);
	if (allocatedParticlesAlign > 1)
	{
		m_allocatedParticles = eg::RoundToNextMultiple(m_allocatedParticles, allocatedParticlesAlign);
	}

	struct MemoryAllocSubBlock
	{
		void** dest;
		size_t numBytes;
		size_t offset;
	};

	std::vector<MemoryAllocSubBlock> subBlocks;

	size_t dataMemoryOffset = 0;

	auto AllocateParticleMemory = [&]<typename T>(T** ptr, size_t elemsPerParticle = 1)
	{
		dataMemoryOffset = eg::RoundToNextMultiple(dataMemoryOffset, std::max(alignof(T), memoryAlignment));

		size_t numBytes = sizeof(T) * m_allocatedParticles * elemsPerParticle;

		subBlocks.push_back(MemoryAllocSubBlock{
			reinterpret_cast<void**>(ptr),
			numBytes,
			dataMemoryOffset,
		});

		dataMemoryOffset += numBytes;
	};

	AllocateParticleMemory(&m_particlesPos1.x);
	AllocateParticleMemory(&m_particlesPos1.y);
	AllocateParticleMemory(&m_particlesPos1.z);
	AllocateParticleMemory(&m_particlesVel1.x);
	AllocateParticleMemory(&m_particlesVel1.y);
	AllocateParticleMemory(&m_particlesVel1.z);
	AllocateParticleMemory(&m_particlesPos2.x);
	AllocateParticleMemory(&m_particlesPos2.y);
	AllocateParticleMemory(&m_particlesPos2.z);
	AllocateParticleMemory(&m_particlesVel2.x);
	AllocateParticleMemory(&m_particlesVel2.y);
	AllocateParticleMemory(&m_particlesVel2.z);
	AllocateParticleMemory(&m_outputBuffer);
	AllocateParticleMemory(&m_particleDensityX);
	AllocateParticleMemory(&m_particleDensityY);
	AllocateParticleMemory(&m_particlesRadius);
	AllocateParticleMemory(&m_particlesGlowTime);
	AllocateParticleMemory(&m_particlesGravity);
	AllocateParticleMemory(&m_particlesGravity2);

	AllocateParticleMemory(&m_numCloseParticles);
	AllocateParticleMemory(&m_closeParticlesIdx, MAX_CLOSE);
	AllocateParticleMemory(&m_closeParticlesDist, MAX_CLOSE);

	m_dataMemory.reset(aligned_alloc(memoryAlignment, dataMemoryOffset));
	std::memset(m_dataMemory.get(), 0, dataMemoryOffset);

	for (const MemoryAllocSubBlock& block : subBlocks)
	{
		*block.dest = reinterpret_cast<char*>(m_dataMemory.get()) + block.offset;
	}

	// Generates random particle radii
	for (uint32_t i = 0; i < m_numParticles; i++)
	{
		m_particlesRadius[i] = radiusDist(initialRng);
	}

	std::fill_n(m_particlesGravity, m_allocatedParticles, (uint8_t)Dir::NegY);
	std::fill_n(m_particlesGravity2, m_allocatedParticles, (uint8_t)Dir::NegY);

	// Copies particle positions
	for (size_t i = 0; i < m_numParticles; i++)
	{
		m_particlesPos1.x[i] = args.particlePositions[i].x;
		m_particlesPos1.y[i] = args.particlePositions[i].y;
		m_particlesPos1.z[i] = args.particlePositions[i].z;
	}

	worldMin = args.minBounds;
	worldSize = args.maxBounds - args.minBounds;
	isVoxelAir = args.isAirBuffer;
	voxelAirStrideZ = worldSize.x * worldSize.y;

	constexpr int GRID_CELLS_MARGIN = 5;
	partGridMin = glm::ivec3(glm::floor(glm::vec3(args.minBounds) / INFLUENCE_RADIUS)) - GRID_CELLS_MARGIN;
	glm::ivec3 partGridMax = glm::ivec3(glm::ceil(glm::vec3(args.maxBounds) / INFLUENCE_RADIUS)) + GRID_CELLS_MARGIN;
	partGridNumCellGroups = ((partGridMax - partGridMin) + (CELL_GROUP_SIZE - 1)) / CELL_GROUP_SIZE;
	partGridNumCells = partGridNumCellGroups * CELL_GROUP_SIZE;
	cellNumParticles.resize(partGridNumCells.x * partGridNumCells.y * partGridNumCells.z);
	cellParticles.resize(cellNumParticles.size());

	particleCells.resize(m_allocatedParticles);

	for (uint32_t i = 1; i < m_numThreads; i++)
	{
		m_workerThreads.emplace_back([this, i] { WorkerThreadTarget(i); });
	}
}

WaterSimulatorImpl::~WaterSimulatorImpl()
{
	m_workerThreadWakeLock.lock();
	m_workerThreadRunIteration = UINT64_MAX;
	m_workerThreadWakeLock.unlock();
	m_workerThreadWakeSignal.notify_all();
	for (std::thread& thread : m_workerThreads)
		thread.join();
}

uint32_t WaterSimulatorImpl::CopyToOutputBuffer()
{
	for (uint32_t i = 0; i < m_numParticles; i++)
	{
		m_outputBuffer[i] = glm::vec4(
			m_particlesPos1.x[i], m_particlesPos1.y[i], m_particlesPos1.z[i],
			static_cast<float>(m_particlesGlowTime[i]));
	}
	return m_numParticles;
}

void* WaterSimulatorImpl::GetOutputBuffer() const
{
	return m_outputBuffer;
}

void* WaterSimulatorImpl::GetGravitiesOutputBuffer(uint32_t& versionOut) const
{
	versionOut = gravityVersion2;
	return m_particlesGravity2;
}

WaterQueryResults WaterSimulatorImpl::Query(const eg::AABB& aabb) const
{
	WaterQueryResults result;

	for (uint32_t p = 0; p < m_numParticles; p++)
	{
		glm::vec3 poslo = m_particlesPos2[p] - m_particlesRadius[p];
		glm::vec3 poshi = m_particlesPos2[p] + m_particlesRadius[p];
		if (!glm::any(glm::greaterThan(aabb.min, poshi)) && !glm::any(glm::lessThan(aabb.max, poslo)))
		{
			result.numIntersecting++;
			result.waterVelocity += m_particlesVel2[p];
			result.buoyancy -= DirectionVector(static_cast<Dir>(m_particlesGravity[p]));
		}
	}

	return result;
}

void WaterSimulatorImpl::SwapBuffers()
{
	std::swap(m_particlesPos1, m_particlesPos2);
	std::swap(m_particlesVel1, m_particlesVel2);

	if (gravityVersion2 != gravityVersion)
	{
		std::memcpy(m_particlesGravity2, m_particlesGravity, m_numParticles);
		gravityVersion2 = gravityVersion;
	}
}

int WaterSimulatorImpl::CellIdx(glm::ivec3 coord) const
{
	if (coord[0] < 0 || coord[1] < 0 || coord[2] < 0 || coord[0] >= partGridNumCells.x ||
	    coord[1] >= partGridNumCells.y || coord[2] >= partGridNumCells.z)
	{
		return -1;
	}

	glm::ivec3 group = glm::ivec3(coord[0], coord[1], coord[2]) / CELL_GROUP_SIZE;
	glm::ivec3 local = glm::ivec3(coord[0], coord[1], coord[2]) - group * CELL_GROUP_SIZE;
	int groupIdx =
		group.x + group.y * partGridNumCellGroups.x + group.z * partGridNumCellGroups.x * partGridNumCellGroups.y;

	return groupIdx * CELL_GROUP_SIZE * CELL_GROUP_SIZE * CELL_GROUP_SIZE + local.x + local.y * CELL_GROUP_SIZE +
	       local.z * CELL_GROUP_SIZE * CELL_GROUP_SIZE;
}

bool WaterSimulatorImpl::IsVoxelAir(glm::ivec3 coord) const
{
	glm::ivec3 rVoxel = coord - worldMin;
	if (rVoxel.x < 0 || rVoxel.y < 0 || rVoxel.z < 0)
		return false;
	if (rVoxel.x >= worldSize.x || rVoxel.y >= worldSize.y || rVoxel.z >= worldSize.z)
		return false;

	size_t idx = rVoxel[0] + rVoxel[1] * worldSize.x + rVoxel[2] * voxelAirStrideZ;
	return isVoxelAir[idx / 8] & (1 << (idx % 8));
}

void WaterSimulatorImpl::MoveAcrossPumps(std::span<const WaterPumpDescription> pumps, float dt)
{
	static constexpr int MAX_PUMP_PER_ITERATION = 16;

	for (const WaterPumpDescription& pump : pumps)
	{
		int numToMove =
			glm::clamp(static_cast<int>(std::round(pump.particlesPerSecond * dt)), 1, MAX_PUMP_PER_ITERATION);

		uint16_t closestIndices[MAX_PUMP_PER_ITERATION];
		float closestDist2[MAX_PUMP_PER_ITERATION];
		std::fill_n(closestIndices, MAX_PUMP_PER_ITERATION, UINT16_MAX);
		std::fill_n(closestDist2, MAX_PUMP_PER_ITERATION, INFINITY);

		// Selects candidate particles ordered by distance to the pump
		for (uint32_t i = 0; i < m_numParticles; i++)
		{
			glm::vec3 sep = pump.source - m_particlesPos1[i];
			float dist2 = glm::length2(sep);
			if (dist2 > pump.maxInputDistSquared)
				continue;

			size_t idx = std::lower_bound(closestDist2, closestDist2 + MAX_PUMP_PER_ITERATION, dist2) - closestDist2;
			if (idx >= (size_t)MAX_PUMP_PER_ITERATION)
				continue;

			std::copy_backward(
				closestIndices + idx, closestIndices + MAX_PUMP_PER_ITERATION - 1,
				closestIndices + MAX_PUMP_PER_ITERATION);
			std::copy_backward(
				closestDist2 + idx, closestDist2 + MAX_PUMP_PER_ITERATION - 1, closestIndices + MAX_PUMP_PER_ITERATION);

			closestIndices[idx] = i;
			closestDist2[idx] = dist2;
		}

		std::uniform_real_distribution<float> offsetDist(-pump.maxOutputDist, pump.maxOutputDist);

		// Moves particles
		for (int i = 0; i < numToMove; i++)
		{
			const uint16_t idx = closestIndices[i];
			if (idx != UINT16_MAX)
			{
				m_particlesPos1.x[idx] = pump.dest.x + offsetDist(m_threadRngs[0]);
				m_particlesPos1.y[idx] = pump.dest.y + offsetDist(m_threadRngs[0]);
				m_particlesPos1.z[idx] = pump.dest.z + offsetDist(m_threadRngs[0]);
				m_particlesVel1.x[idx] = 0;
				m_particlesVel1.y[idx] = 0;
				m_particlesVel1.z[idx] = 0;
			}
		}
	}
}

// Changes gravity for particles.
//  This is done by running a DFS across the graph of close particles, starting at the changed particle.
void WaterSimulatorImpl::ChangeParticleGravity(glm::vec3 changePos, bool highlightOnly, Dir newGravity, float gameTime)
{
	// Finds the particle closest to the change gravity particle position
	size_t changeGravityParticle = SIZE_MAX;
	float closestParticleDist2 = INFINITY;
	for (uint32_t i = 0; i < m_numParticles; i++)
	{
		float dist2 = glm::distance2(changePos, m_particlesPos1[i]);
		if (dist2 < closestParticleDist2)
		{
			closestParticleDist2 = dist2;
			changeGravityParticle = i;
		}
	}

	if (changeGravityParticle != SIZE_MAX)
	{
		std::vector<bool> seen(m_numParticles, false);
		seen[changeGravityParticle] = true;
		std::vector<uint16_t> particlesStack;
		particlesStack.reserve(m_numParticles);
		particlesStack.push_back(eg::UnsignedNarrow<uint16_t>(changeGravityParticle));
		while (!particlesStack.empty())
		{
			uint16_t cur = particlesStack.back();
			particlesStack.pop_back();

			if (!highlightOnly)
			{
				m_particlesGravity[cur] = static_cast<uint8_t>(newGravity);
			}
			m_particlesGlowTime[cur] = gameTime;

			for (uint16_t bI = 0; bI < m_numCloseParticles[cur]; bI++)
			{
				uint16_t b = GetCloseParticlesIdxPtr(cur)[bI];
				if (!seen[b])
				{
					particlesStack.push_back(b);
					seen[b] = true;
				}
			}
		}

		if (!highlightOnly)
		{
			gravityVersion++;
		}
	}
}

std::pair<uint32_t, uint32_t> WaterSimulatorImpl::GetThreadWorkingRange(uint32_t threadIndex) const
{
	uint32_t itemsPerThread = m_numParticles / m_numThreads;
	itemsPerThread &= ~(m_itemsPerThreadPreferredDivisibility - 1);

	uint32_t lo = itemsPerThread * threadIndex;
	uint32_t hi = lo + itemsPerThread;
	if (threadIndex == m_numThreads - 1)
	{
		hi = m_numParticles;
	}
	return { lo, hi };
}

const glm::vec3 WaterSimulatorImpl::gravities[6] = {
	glm::vec3(WATER_GRAVITY, 0, 0),  glm::vec3(-WATER_GRAVITY, 0, 0), glm::vec3(0, WATER_GRAVITY, 0),
	glm::vec3(0, -WATER_GRAVITY, 0), glm::vec3(0, 0, WATER_GRAVITY),  glm::vec3(0, 0, -WATER_GRAVITY),
};

static const glm::ivec3 voxelNormalsI[6] = {
	glm::ivec3(-1, 0, 0), glm::ivec3(1, 0, 0),  glm::ivec3(0, -1, 0),
	glm::ivec3(0, 1, 0),  glm::ivec3(0, 0, -1), glm::ivec3(0, 0, 1),
};
static const glm::vec3 voxelNormalsF[6] = {
	glm::vec3(-1, 0, 0), glm::vec3(1, 0, 0),  glm::vec3(0, -1, 0),
	glm::vec3(0, 1, 0),  glm::vec3(0, 0, -1), glm::vec3(0, 0, 1),
};

static const glm::ivec3 voxelTangentsI[6] = {
	glm::ivec3(0, 0, 1), glm::ivec3(0, 0, 1), glm::ivec3(1, 0, 0),
	glm::ivec3(1, 0, 0), glm::ivec3(0, 1, 0), glm::ivec3(0, 1, 0),
};
static const glm::vec3 voxelTangentsF[6] = {
	glm::vec3(0, 0, 1), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0),
	glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0),
};

static const glm::ivec3 voxelBitangentsI[6] = {
	glm::ivec3(0, 0, 1), glm::ivec3(0, 0, 1), glm::ivec3(1, 0, 0),
	glm::ivec3(1, 0, 0), glm::ivec3(0, 1, 0), glm::ivec3(0, 1, 0),
};
static const glm::vec3 voxelBitangentsF[6] = {
	glm::vec3(0, 1, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1),
	glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(1, 0, 0),
};

void WaterSimulatorImpl::WorkerThreadTarget(uint32_t threadIndex)
{
	uint64_t oldIteration = 0;
	while (true)
	{
		std::unique_lock<std::mutex> lock(m_workerThreadWakeLock);
		m_workerThreadWakeSignal.wait(lock, [&] { return m_workerThreadRunIteration != oldIteration; });
		if (m_workerThreadRunIteration == UINT64_MAX)
			return;
		oldIteration = m_workerThreadRunIteration;
		float dt = m_dtForWorkerThread;
		std::span<const WaterBlocker> waterBlockers = m_waterBlockersForWorkerThread;
		lock.unlock();

		RunAllParallelizedSimulationStages(threadIndex, dt, waterBlockers);
	}
}

void WaterSimulatorImpl::RunAllParallelizedSimulationStages(
	uint32_t threadIndex, float dt, std::span<const WaterBlocker> waterBlockers)
{
	auto [plo, phi] = GetThreadWorkingRange(threadIndex);
	SimulateStageArgs simulateStageArgs = { .threadIndex = threadIndex, .loIdx = plo, .hiIdx = phi, .dt = dt };

	Stage1_DetectClose(simulateStageArgs);
	m_barrier.arrive_and_wait();
	Stage2_ComputeNumberDensity(simulateStageArgs);
	m_barrier.arrive_and_wait();
	Stage3_Acceleration(simulateStageArgs);
	m_barrier.arrive_and_wait();
	Stage4_DiffusionAndCollision(simulateStageArgs, waterBlockers);
	m_barrier.arrive_and_wait();
}

// Detects close particles by scanning the surrounding grid cells for each particle.
void WaterSimulatorImpl::Stage1_DetectClose(SimulateStageArgs args)
{
	for (uint32_t p = args.loIdx; p < args.hiIdx; p++)
	{
		glm::ivec3 centerCell = particleCells[p];
		uint32_t numClose = 0;

		// Loop through neighboring cells to the one the current particle belongs to
		for (int dx = -1; dx <= 1; dx++)
		{
			for (int dy = -1; dy <= 1; dy++)
			{
				for (int dz = -1; dz <= 1; dz++)
				{
					int cell = CellIdx(centerCell + glm::ivec3(dx, dy, dz));
					if (cell == -1)
						continue;
					for (int i = 0; i < cellNumParticles[cell]; i++)
					{
						uint16_t b = cellParticles[cell][i];
						if (b != p)
						{
							// Check particle b for proximity
							float sepX = m_particlesPos1.x[p] - m_particlesPos1.x[b];
							float sepY = m_particlesPos1.y[p] - m_particlesPos1.y[b];
							float sepZ = m_particlesPos1.z[p] - m_particlesPos1.z[b];

							float dist2 = sepX * sepX + sepY * sepY + sepZ * sepZ;
							if (dist2 < INFLUENCE_RADIUS * INFLUENCE_RADIUS)
							{
								GetCloseParticlesIdxPtr(p)[numClose] = b;
								GetCloseParticlesDistPtr(p)[numClose] = std::sqrt(dist2);
								numClose++;
								if (numClose == MAX_CLOSE)
									break;
							}
						}
					}
					if (numClose == MAX_CLOSE)
						break;
				}
			}
		}

		m_numCloseParticles[p] = numClose;
	}
}

void WaterSimulatorImpl::Stage2_ComputeNumberDensity(SimulateStageArgs args)
{
	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		float densityX = 1.0f;
		float densityY = 1.0f;

		for (uint16_t bI = 0; bI < m_numCloseParticles[a]; bI++)
		{
			const float dist = GetCloseParticlesDistPtr(a)[bI];
			const float q = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2 = q * q;
			const float q3 = q2 * q;
			const float q4 = q2 * q2;
			densityX += q3;
			densityY += q4;
		}

		m_particleDensityX[a] = densityX;
		m_particleDensityY[a] = densityY;
	}
}

void WaterSimulatorImpl::Stage3_Acceleration(SimulateStageArgs args)
{
	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		// Initializes acceleration to the gravitational acceleration
		const float densA = m_particleDensityX[a];
		const float nearDensA = m_particleDensityY[a];
		const float relativeDensity = (densA - AMBIENT_DENSITY) / densA;
		glm::vec3 accel = relativeDensity * gravities[m_particlesGravity[a]];

		const float aDensityXSub2TargetDensity = densA - 2 * TARGET_NUMBER_DENSITY;

		// Applies acceleration due to pressure
		for (uint16_t bI = 0; bI < m_numCloseParticles[a]; bI++)
		{
			uint16_t b = GetCloseParticlesIdxPtr(a)[bI];
			float dist = GetCloseParticlesDistPtr(a)[bI];

			float sepX = m_particlesPos1.x[a] - m_particlesPos1.x[b];
			float sepY = m_particlesPos1.y[a] - m_particlesPos1.y[b];
			float sepZ = m_particlesPos1.z[a] - m_particlesPos1.z[b];

			// Randomly separates particles that are very close so that the pressure gradient won't be zero.
			if (dist < CORE_RADIUS || std::abs(sepX) < CORE_RADIUS || std::abs(sepY) < CORE_RADIUS ||
			    std::abs(sepZ) < CORE_RADIUS)
			{
				std::uniform_real_distribution<float> offsetDist(-CORE_RADIUS / 2, CORE_RADIUS);
				sepX += offsetDist(m_threadRngs[args.threadIndex]);
				sepY += offsetDist(m_threadRngs[args.threadIndex]);
				sepZ += offsetDist(m_threadRngs[args.threadIndex]);
				dist = std::sqrt(sepX * sepX + sepY * sepY + sepZ * sepZ);
			}

			const float q = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2 = q * q;
			const float q3 = q2 * q;

			const float densB = m_particleDensityX[b];
			const float nearDensB = m_particleDensityY[b];
			const float pressure = STIFFNESS * (aDensityXSub2TargetDensity + densB);
			const float pressNear = STIFFNESS * NEAR_TO_FAR * (nearDensA + nearDensB);
			const float accelScale = (pressure * q2 + pressNear * q3) / (dist + FLT_EPSILON);
			accel += glm::vec3(sepX, sepY, sepZ) * accelScale;
		}

		// Applies acceleration to the particle's velocity
		glm::vec3 velAdd = accel * args.dt;
		m_particlesVel1.x[a] += velAdd.x;
		m_particlesVel1.y[a] += velAdd.y;
		m_particlesVel1.z[a] += velAdd.z;
	}
}

void WaterSimulatorImpl::Stage4_DiffusionAndCollision(
	SimulateStageArgs args, std::span<const WaterBlocker> waterBlockers)
{
	for (uint32_t a = args.loIdx; a < args.hiIdx; a++)
	{
		glm::vec3 vel = m_particlesVel1[a];

		uint8_t particleGravityMask = (uint8_t)1 << m_particlesGravity[a];

		// Velocity diffusion
		for (uint16_t bI = 0; bI < m_numCloseParticles[a]; bI++)
		{
			const uint16_t b = GetCloseParticlesIdxPtr(a)[bI];
			const glm::vec3 velA = m_particlesVel1[a];
			const glm::vec3 velB = m_particlesVel1[b];

			const float dist = GetCloseParticlesDistPtr(a)[bI];

			const float sepX = m_particlesPos1.x[a] - m_particlesPos1.x[b];
			const float sepY = m_particlesPos1.y[a] - m_particlesPos1.y[b];
			const float sepZ = m_particlesPos1.z[a] - m_particlesPos1.z[b];
			const glm::vec3 sepDir = glm::vec3(sepX, sepY, sepZ) / dist;
			const glm::vec3 velDiff = velA - velB;
			const float velSep = glm::dot(velDiff, sepDir);
			if (velSep < 0.0f)
			{
				const float infl = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
				const float velSepA = glm::dot(velA, sepDir);
				const float velSepB = glm::dot(velB, sepDir);
				const float velSepTarget = (velSepA + velSepB) * 0.5f;
				const float diffSepA = velSepTarget - velSepA;
				const float changeSepA = RADIAL_VISCOSITY_GAIN * diffSepA * infl;
				const glm::vec3 changeA = changeSepA * sepDir;
				vel += changeA;
			}
		}

		// Applies the velocity to the particle's position
		constexpr float VEL_SCALE = 0.998f;
		constexpr float MAX_MOVE = 0.2f;
		vel *= VEL_SCALE;
		glm::vec3 move = vel * args.dt;
		float moveDistSquared = glm::length2(move);
		if (moveDistSquared > MAX_MOVE * MAX_MOVE)
		{
			move *= MAX_MOVE / std::sqrt(moveDistSquared);
		}
		m_particlesPos2.x[a] = m_particlesPos1.x[a] + move.x;
		m_particlesPos2.y[a] = m_particlesPos1.y[a] + move.y;
		m_particlesPos2.z[a] = m_particlesPos1.z[a] + move.z;

		glm::vec3 halfM = glm::vec3(0.5f);

		constexpr int COLLISION_DETECTION_ITERATIONS = 4;

		// Collision detection. This works by looping over all voxels close to the particle (within CHECK_RAD),
		// then checking all faces that face from a solid voxel to an air voxel.
		for (int tr = 0; tr < COLLISION_DETECTION_ITERATIONS; tr++)
		{
			glm::ivec3 centerVx = glm::ivec3(glm::floor(m_particlesPos2[a]));

			float minDist = 0;
			glm::vec3 displaceNormal = glm::vec3(0.0f);

			auto CheckFace = [&](glm::vec3 planePoint, glm::vec3 normal, glm::vec3 tangent, glm::vec3 bitangent,
			                     float tangentLen, float biTangentLen, float minDistC)
			{
				float distC = glm::dot((m_particlesPos2[a] - planePoint), normal);
				float distE = distC - m_particlesRadius[a] * 0.5f;

				if (distC > minDistC && distE < minDist)
				{
					glm::vec3 iPos = m_particlesPos2[a] + distC * normal - planePoint;
					float iDotT = glm::dot(iPos, tangent);
					float iDotBT = glm::dot(iPos, bitangent);
					float iDotN = glm::dot(iPos, normal);

					// Checks that the intersection actually happened on the voxel's face
					if (std::abs(iDotT) <= tangentLen && std::abs(iDotBT) <= biTangentLen && std::abs(iDotN) < 0.8f)
					{
						minDist = distE - 0.001f;
						displaceNormal = normal;
					}
				}
			};

			for (const WaterBlocker& blocker : waterBlockers)
			{
				if (blocker.blockedGravities & particleGravityMask)
				{
					CheckFace(
						blocker.center, blocker.normal, blocker.tangent, blocker.biTangent, blocker.tangentLen,
						blocker.biTangentLen, 0);
				}
			}

			constexpr int CHECK_RAD = 1;
			for (int dz = -CHECK_RAD; dz <= CHECK_RAD; dz++)
			{
				for (int dy = -CHECK_RAD; dy <= CHECK_RAD; dy++)
				{
					for (int dx = -CHECK_RAD; dx <= CHECK_RAD; dx++)
					{
						glm::ivec3 voxelCoordSolid = centerVx + glm::ivec3(dx, dy, dz);

						// This voxel must be solid
						if (IsVoxelAir(voxelCoordSolid))
							continue;

						for (size_t n = 0; n < std::size(voxelNormalsI); n++)
						{
							glm::ivec3 normal = voxelNormalsI[n];

							// This voxel cannot be solid
							if (!IsVoxelAir(voxelCoordSolid + normal))
								continue;

							glm::vec3 normalF = voxelNormalsF[n];

							// A point on the plane going through this face
							glm::vec3 planePoint = glm::vec3(voxelCoordSolid) + halfM + normalF * halfM;

							glm::ivec3 tangent = voxelTangentsI[n];
							glm::ivec3 bitangent = voxelBitangentsI[n];
							float tangentLen =
								!IsVoxelAir(voxelCoordSolid + tangent) && !IsVoxelAir(voxelCoordSolid - tangent) ? 0.6f
																												 : 0.5f;
							float bitangentLen =
								!IsVoxelAir(voxelCoordSolid + bitangent) && !IsVoxelAir(voxelCoordSolid - bitangent)
									? 0.6f
									: 0.5f;

							CheckFace(
								planePoint, normalF, voxelTangentsF[n], voxelBitangentsF[n], tangentLen, bitangentLen,
								-INFINITY);
						}
					}
				}
			}

			// Applies an impulse to the velocity
			if (minDist < 0)
			{
				float vDotDisplace = glm::dot(vel, displaceNormal) * IMPACT_COEFFICIENT;
				glm::vec3 impulse = displaceNormal * vDotDisplace;
				vel -= impulse;
			}

			// Applies collision correction
			m_particlesPos2.x[a] -= displaceNormal.x * minDist;
			m_particlesPos2.y[a] -= displaceNormal.y * minDist;
			m_particlesPos2.z[a] -= displaceNormal.z * minDist;
		}

		m_particlesVel2.x[a] = vel.x;
		m_particlesVel2.y[a] = vel.y;
		m_particlesVel2.z[a] = vel.z;
	}
}

void WaterSimulatorImpl::Simulate(const SimulateArgs& args)
{
	MoveAcrossPumps(args.waterPumps, args.dt);

	std::memset(cellNumParticles.data(), 0, cellNumParticles.size() * sizeof(uint16_t));

	// Partitions particles into grid cells so that detecting close particles will be faster.
	for (uint32_t i = 0; i < m_numParticles; i++)
	{
		// Computes the cell to place this particle into based on the floor of the particle's position.
		particleCells[i] = glm::ivec3(glm::floor(m_particlesPos1[i] / INFLUENCE_RADIUS)) - partGridMin;

		// Adds the particle to the cell
		int cell = CellIdx(particleCells[i]);
		if (cell != -1 && cellNumParticles[cell] < MAX_PER_PART_CELL)
		{
			cellParticles[cell][cellNumParticles[cell]++] = i;
		}
	}

	if (args.shouldChangeParticleGravity)
	{
		ChangeParticleGravity(
			args.changeGravityParticlePos, args.changeGravityParticleHighlightOnly, args.newGravity, args.gameTime);
	}

	{
		std::lock_guard<std::mutex> lock(m_workerThreadWakeLock);
		m_dtForWorkerThread = args.dt;
		m_waterBlockersForWorkerThread = args.waterBlockers;
		EG_ASSERT(m_workerThreadRunIteration != UINT64_MAX);
		m_workerThreadRunIteration++;
	}
	m_workerThreadWakeSignal.notify_all();

	RunAllParallelizedSimulationStages(0, args.dt, args.waterBlockers);
}
