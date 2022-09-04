#ifdef IOMOMI_ENABLE_WATER
#include <cstdlib>
#include <execution>
#include <pcg_random.hpp>
#include <glm/gtx/fast_square_root.hpp>

#include "WaterSimulatorImpl.hpp"

#ifdef _WIN32
inline static void* aligned_alloc(size_t alignment, size_t size)
{
	return _aligned_malloc(size, alignment);
}
#define ALIGNED_FREE _aligned_free
#else
#define ALIGNED_FREE std::free
#endif

//Maximum number of particles per partition grid cell
static constexpr uint32_t MAX_PER_PART_CELL = 512;

//Maximum number of close particles
static constexpr uint32_t MAX_CLOSE = 512;

static constexpr float ELASTICITY = 0.2f;
static constexpr float IMPACT_COEFFICIENT = 1.0f + ELASTICITY;
static constexpr float MIN_PARTICLE_RADIUS = 0.1f;
static constexpr float MAX_PARTICLE_RADIUS = 0.4f;
static constexpr float INFLUENCE_RADIUS = 0.6f;
static constexpr float CORE_RADIUS = 0.001f;
static constexpr float RADIAL_VISCOSITY_GAIN = 0.25f;
static constexpr float TARGET_NUMBER_DENSITY = 2.5f;
static constexpr float STIFFNESS = 20.0f;
static constexpr float NEAR_TO_FAR = 2.0f;
static constexpr float AMBIENT_DENSITY = 0.5f;
static constexpr int CELL_GROUP_SIZE = 4;

static std::uniform_real_distribution<float> radiusDist(MIN_PARTICLE_RADIUS, MAX_PARTICLE_RADIUS);

struct WaterSimulatorImpl
{
	alignas(16) int32_t cellProcOffsets[4 * 3 * 3 * 3];
	
	pcg32_fast rng;
	
	uint32_t numParticles;
	uint32_t allocatedParticles;
	uint32_t particleGravityVersion;
	uint32_t particleGravityOutVersion;
	
	//Memory for particle data
	void* dataMemory;
	
	//SoA particle data, points into dataMemory
	__m128* particlePos;
	__m128* particlePos2;
	__m128* particlePosOut;
	__m128* particleVel;
	__m128* particleVel2;
	float* particleRadius;
	uint8_t* particleGravity;
	uint8_t* particleGravityOut;
	glm::vec2* particleDensity;
	
	//Stores which cell each particle belongs to
	__m128i* particleCells;
	
	//For each partition cell, stores how many particles belong to that cell.
	std::vector<uint16_t> cellNumParticles;
	
	//For each partition cell, stores a list of particle indices that belong to that cell.
	std::vector<std::array<uint16_t, MAX_PER_PART_CELL>> cellParticles;
	
	__m128i worldMin;
	__m128i worldSizeMinus1;
	int voxelAirStrideZ;
	uint8_t* isVoxelAir;
	
	glm::ivec3 worldSize;
	
	glm::ivec3 partGridMin;
	glm::ivec3 partGridNumCellGroups;
	glm::ivec3 partGridNumCells;
	
	std::vector<uint16_t> numCloseParticles;
	std::vector<std::array<uint16_t, MAX_CLOSE>> closeParticles;
};

WaterSimulatorImpl* WSI_New(const WSINewArgs& args)
{
	WaterSimulatorImpl* impl = new WaterSimulatorImpl;
	
	for (int z = 0; z < 3; z++)
	{
		for (int y = 0; y < 3; y++)
		{
			for (int x = 0; x < 3; x++)
			{
				int baseIdx = x + y * 3 + z * 9;
				impl->cellProcOffsets[baseIdx * 4 + 0] = x - 1;
				impl->cellProcOffsets[baseIdx * 4 + 1] = y - 1;
				impl->cellProcOffsets[baseIdx * 4 + 2] = z - 1;
				impl->cellProcOffsets[baseIdx * 4 + 3] = 0;
			}
		}
	}
	
	impl->worldMin = _mm_set_epi32(0, args.minBounds.z, args.minBounds.y, args.minBounds.x);
	impl->worldSize = args.maxBounds - args.minBounds;
	impl->worldSizeMinus1 = _mm_set_epi32(0, impl->worldSize.z - 1, impl->worldSize.y - 1, impl->worldSize.x - 1);
	impl->voxelAirStrideZ = impl->worldSize.x * impl->worldSize.y;
	impl->isVoxelAir = args.isAirBuffer;
	impl->numParticles = args.numParticles;
	impl->allocatedParticles = args.numParticles + args.extraParticles;
	impl->particleGravityVersion = 0;
	impl->particleGravityOutVersion = 0;
	
	constexpr int GRID_CELLS_MARGIN = 5;
	impl->partGridMin = glm::ivec3(glm::floor(glm::vec3(args.minBounds) / INFLUENCE_RADIUS)) - GRID_CELLS_MARGIN;
	glm::ivec3 partGridMax = glm::ivec3(glm::ceil(glm::vec3(args.maxBounds) / INFLUENCE_RADIUS)) + GRID_CELLS_MARGIN;
	impl->partGridNumCellGroups = ((partGridMax - impl->partGridMin) + (CELL_GROUP_SIZE - 1)) / CELL_GROUP_SIZE;
	impl->partGridNumCells = impl->partGridNumCellGroups * CELL_GROUP_SIZE;
	impl->cellNumParticles.resize(impl->partGridNumCells.x * impl->partGridNumCells.y * impl->partGridNumCells.z);
	impl->cellParticles.resize(impl->cellNumParticles.size());
	
	//Allocates memory for particle data
	size_t memoryBytes = impl->allocatedParticles * (sizeof(__m128) * 6 + sizeof(uint8_t) * 2 + sizeof(float) + sizeof(glm::vec2));
	impl->dataMemory = aligned_alloc(alignof(__m128), memoryBytes);
	std::memset(impl->dataMemory, 0, memoryBytes);
	
	size_t dataMemoryOffset = 0;
	auto AllocateParticleMemory = [&] <typename T> (T** ptr) {
		EG_ASSERT((dataMemoryOffset % alignof(T)) == 0);
		*ptr = reinterpret_cast<T*>(static_cast<char*>(impl->dataMemory) + dataMemoryOffset);
		dataMemoryOffset += sizeof(T) * impl->allocatedParticles;
	};
	
	//Sets up SoA particle data
	AllocateParticleMemory(&impl->particlePos);
	AllocateParticleMemory(&impl->particlePos2);
	AllocateParticleMemory(&impl->particlePosOut);
	AllocateParticleMemory(&impl->particleVel);
	AllocateParticleMemory(&impl->particleVel2);
	AllocateParticleMemory(&impl->particleCells);
	AllocateParticleMemory(&impl->particleRadius);
	AllocateParticleMemory(&impl->particleDensity);
	AllocateParticleMemory(&impl->particleGravity);
	AllocateParticleMemory(&impl->particleGravityOut);
	if (dataMemoryOffset != memoryBytes)
		std::abort();
	
	//Generates random particle radii
	for (uint32_t i = 0; i < impl->numParticles; i++)
		impl->particleRadius[i] = radiusDist(impl->rng);
	
	std::fill_n(impl->particleGravity, impl->numParticles, (uint8_t)Dir::NegY);
	std::fill_n(impl->particleGravityOut, impl->numParticles, (uint8_t)Dir::NegY);
	
	//Copies particle positions to m_particlePos
	alignas(16) float positionLoadBuffer[4] = {};
	for (size_t i = 0; i < args.numParticles; i++)
	{
		std::copy_n(args.particlePositions + i * 3, 3, positionLoadBuffer);
		impl->particlePos[i] = _mm_load_ps(positionLoadBuffer);
	}
	
	impl->closeParticles.resize(impl->allocatedParticles);
	impl->numCloseParticles.resize(impl->allocatedParticles);
	
	return impl;
}

void WSI_Delete(WaterSimulatorImpl* impl)
{
	ALIGNED_FREE(impl->dataMemory);
	std::free(impl->isVoxelAir);
	delete impl;
}

uint32_t WSI_CopyToOutputBuffer(WaterSimulatorImpl* impl)
{
	std::memcpy(impl->particlePosOut, impl->particlePos, impl->numParticles * sizeof(float) * 4);
	return impl->numParticles;
}

static constexpr int DP_IMM8 = 0x71;

void WSI_SortOutputBuffer(WaterSimulatorImpl* impl, uint32_t numParticles, const float* cameraPos4)
{
	std::sort(impl->particlePosOut, impl->particlePosOut + numParticles, [&] (const __m128& a, const __m128& b)
	{
		__m128 v1 = _mm_sub_ps(a, *reinterpret_cast<const __m128*>(cameraPos4));
		__m128 v2 = _mm_sub_ps(b, *reinterpret_cast<const __m128*>(cameraPos4));
		return _mm_cvtss_f32(_mm_dp_ps(v1, v1, DP_IMM8)) < _mm_cvtss_f32(_mm_dp_ps(v2, v2, DP_IMM8));
	});
}

void* WSI_GetOutputBuffer(WaterSimulatorImpl* impl)
{
	return impl->particlePosOut;
}

void* WSI_GetGravitiesOutputBuffer(WaterSimulatorImpl* impl, uint32_t& versionOut)
{
	versionOut = impl->particleGravityOutVersion;
	return impl->particleGravityOut;
}

void WSI_SwapBuffers(WaterSimulatorImpl* impl)
{
	std::swap(impl->particleVel2, impl->particleVel);
	std::swap(impl->particlePos2, impl->particlePos);
	
	if (impl->particleGravityOutVersion != impl->particleGravityVersion)
	{
		std::memcpy(impl->particleGravityOut, impl->particleGravity, impl->numParticles);
		impl->particleGravityOutVersion = impl->particleGravityVersion;
	}
}

inline int CellIdx(WaterSimulatorImpl* impl, __m128i coord4)
{
	alignas(16) int32_t coord[4];
	_mm_store_si128((__m128i*)coord, coord4);
	
	if (coord[0] < 0 || coord[1] < 0 || coord[2] < 0 ||
	    coord[0] >= impl->partGridNumCells.x || coord[1] >= impl->partGridNumCells.y || coord[2] >= impl->partGridNumCells.z)
	{
		return -1;
	}
	
	glm::ivec3 group = glm::ivec3(coord[0], coord[1], coord[2]) / CELL_GROUP_SIZE;
	glm::ivec3 local = glm::ivec3(coord[0], coord[1], coord[2]) - group * CELL_GROUP_SIZE;
	int groupIdx = group.x + group.y * impl->partGridNumCellGroups.x + group.z * impl->partGridNumCellGroups.x * impl->partGridNumCellGroups.y;
	
	return groupIdx * CELL_GROUP_SIZE * CELL_GROUP_SIZE * CELL_GROUP_SIZE +
	       local.x + local.y * CELL_GROUP_SIZE + local.z * CELL_GROUP_SIZE * CELL_GROUP_SIZE;
}

inline bool IsVoxelAir(WaterSimulatorImpl* impl, __m128i voxel)
{
	__m128i rVoxel = _mm_sub_epi32(voxel, impl->worldMin);
	
	EG_DEBUG_ASSERT(_mm_extract_epi32(rVoxel, 3) == 0);
	
	if (_mm_movemask_epi8(_mm_cmplt_epi32(rVoxel, _mm_setzero_si128())))
		return false;
	if (_mm_movemask_epi8(_mm_cmpgt_epi32(rVoxel, impl->worldSizeMinus1)))
		return false;
	
	alignas(16) int32_t rVoxelI[4];
	_mm_store_si128((__m128i*)rVoxelI, rVoxel);
	size_t idx = rVoxelI[0] + rVoxelI[1] * impl->worldSize.x + rVoxelI[2] * impl->voxelAirStrideZ;
	return impl->isVoxelAir[idx / 8] & (1 << (idx % 8));
}

int WSI_Query(WaterSimulatorImpl* impl, const eg::AABB& aabb, glm::vec3& waterVelocity, glm::vec3& buoyancy)
{
	__m128 aabbMin = _mm_set_ps(-INFINITY, aabb.min.z, aabb.min.y, aabb.min.x);
	__m128 aabbMax = _mm_set_ps(INFINITY, aabb.max.z, aabb.max.y, aabb.max.x);
	__m128 querylo = _mm_min_ps(aabbMin, aabbMax);
	__m128 queryhi = _mm_max_ps(aabbMin, aabbMax);
	
	__m128 waterVelM = _mm_setzero_ps();
	
	int numIntersecting = 0;
	for (uint32_t p = 0; p < impl->numParticles; p++)
	{
		__m128 particleRadius = _mm_set1_ps(impl->particleRadius[p]);
		__m128 poslo = _mm_sub_ps(impl->particlePos2[p], particleRadius);
		__m128 poshi = _mm_add_ps(impl->particlePos2[p], particleRadius);
		if (_mm_movemask_ps(_mm_or_ps(_mm_cmpgt_ps(querylo, poshi), _mm_cmplt_ps(queryhi, poslo))) == 0)
		{
			numIntersecting++;
			waterVelM = _mm_add_ps(waterVelM, impl->particleVel2[p]);
			buoyancy -= DirectionVector(static_cast<Dir>(impl->particleGravity[p]));
		}
	}
	
	alignas(16) float waterVelBuffer[4];
	_mm_store_ps(waterVelBuffer, waterVelM);
	waterVelocity.x = waterVelBuffer[0];
	waterVelocity.y = waterVelBuffer[1];
	waterVelocity.z = waterVelBuffer[2];
	
	return numIntersecting;
}

static constexpr float GRAVITY = 10;
alignas(16) static const float gravities[6][4] =
{
	{  GRAVITY, 0, 0, 0 },
	{ -GRAVITY, 0, 0, 0 },
	{ 0,  GRAVITY, 0, 0 },
	{ 0, -GRAVITY, 0, 0 },
	{ 0, 0,  GRAVITY, 0 },
	{ 0, 0, -GRAVITY, 0 }
};

alignas(16) static const int32_t voxelNormalsI[][4] =
{
	{ -1, 0, 0, 0}, { 1, 0, 0, 0 },
	{ 0, -1, 0, 0}, { 0, 1, 0, 0 },
	{ 0, 0, -1, 0}, { 0, 0, 1, 0 }
};
alignas(16) static const float voxelNormalsF[][4] =
{
	{ -1, 0, 0, 0}, { 1, 0, 0, 0 },
	{ 0, -1, 0, 0}, { 0, 1, 0, 0 },
	{ 0, 0, -1, 0}, { 0, 0, 1, 0 }
};

alignas(16) static const int32_t voxelTangentsI[][4] =
{
	{ 0, 0, 1, 0 }, { 0, 0, 1, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 0, 1, 0, 0 }, { 0, 1, 0, 0 }
};
alignas(16) static const float voxelTangentsF[][4] =
{
	{ 0, 0, 1, 0 }, { 0, 0, 1, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 0, 1, 0, 0 }, { 0, 1, 0, 0 }
};

alignas(16) static const int32_t voxelBitangentsI[][4] =
{
	{ 0, 0, 1, 0 }, { 0, 0, 1, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 },
	{ 0, 1, 0, 0 }, { 0, 1, 0, 0 }
};
alignas(16) static const float voxelBitangentsF[][4] =
{
	{ 0, 1, 0, 0 }, { 0, 1, 0, 0 },
	{ 0, 0, 1, 0 }, { 0, 0, 1, 0 },
	{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }
};

constexpr int MAX_PUMP_PER_ITERATION = 8;

__m128i and3Comp = _mm_set_epi32(0, -1, -1, -1);

void WSI_Simulate(WaterSimulatorImpl* impl, const WSISimulateArgs& args)
{
	float dt = args.dt;
	
	//Moves particles across pumps
	for (const WaterPumpDescription& pump : args.waterPumps)
	{
		int numToMove = glm::clamp(
			static_cast<int>(std::round(pump.particlesPerSecond * args.dt)),
			1, MAX_PUMP_PER_ITERATION);
		
		uint16_t closestIndices[MAX_PUMP_PER_ITERATION];
		float closestDist2[MAX_PUMP_PER_ITERATION];
		std::fill_n(closestIndices, MAX_PUMP_PER_ITERATION, UINT16_MAX);
		std::fill_n(closestDist2, MAX_PUMP_PER_ITERATION, INFINITY);
		
		__m128 sourcePos = { pump.source.x, pump.source.y, pump.source.z, 0 };
		
		//Selects candidate particles ordered by distance to the pump
		for (size_t i = 0; i < impl->numParticles; i++)
		{
			__m128 sep = _mm_sub_ps(sourcePos, impl->particlePos[i]);
			float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8));
			if (dist2 > pump.maxInputDistSquared)
				continue;
			
			size_t idx = std::lower_bound(closestDist2, closestDist2 + MAX_PUMP_PER_ITERATION, dist2) - closestDist2;
			if (idx >= (size_t)MAX_PUMP_PER_ITERATION)
				continue;
			
			for (size_t j = (size_t)MAX_PUMP_PER_ITERATION - 1; j > idx; j++)
			{
				closestIndices[j] = closestIndices[j - 1];
				closestDist2[j] = closestDist2[j - 1];
			}
			closestIndices[idx] = i;
			closestDist2[idx] = dist2;
		}
		
		std::uniform_real_distribution<float> offsetDist(-pump.maxOutputDist, pump.maxOutputDist);
		
		//Moves particles
		for (int i = 0; i < numToMove; i++)
		{
			const uint16_t idx = closestIndices[i];
			if (idx != UINT16_MAX)
			{
				alignas(16) float newPos[4] = {
					pump.dest.x + offsetDist(impl->rng),
					pump.dest.y + offsetDist(impl->rng),
					pump.dest.z + offsetDist(impl->rng),
					0.0f
				};
				impl->particlePos[idx] = _mm_load_ps(newPos);
				impl->particleVel[idx] = _mm_setzero_ps();
			}
		}
	}
	
	std::memset(impl->cellNumParticles.data(), 0, impl->cellNumParticles.size() * sizeof(uint16_t));
	
	__m128 gridCell4 = _mm_set1_ps(INFLUENCE_RADIUS);
	alignas(16) const int32_t gridCellsMin4[] = { impl->partGridMin.x, impl->partGridMin.y, impl->partGridMin.z, 0 };
	
	//Partitions particles into grid cells so that detecting close particles will be faster.
	for (uint32_t i = 0; i < impl->numParticles; i++)
	{
		impl->particleDensity[i] = glm::vec2(1.0f);
		
		//Computes the cell to place this particle into based on the floor of the particle's position.
		impl->particleCells[i] = _mm_sub_epi32(
			_mm_cvtps_epi32(_mm_floor_ps(_mm_div_ps(impl->particlePos[i], gridCell4))),
			*reinterpret_cast<const __m128i*>(gridCellsMin4));
		
		//Adds the particle to the cell
		int cell = CellIdx(impl, impl->particleCells[i]);
		if (cell != -1 && impl->cellNumParticles[cell] < MAX_PER_PART_CELL)
		{
			impl->cellParticles[cell][impl->cellNumParticles[cell]++] = i;
		}
	}
	
	//Detects close particles by scanning the surrounding grid cells for each particle.
	std::for_each_n(std::execution::par_unseq, impl->particlePos, impl->numParticles, [&] (__m128& particlePos)
	{
		uint32_t p = &particlePos - impl->particlePos;
		__m128i centerCell = impl->particleCells[p];
		uint32_t numClose = 0;
		
		//Loop through neighboring cells to the one the current particle belongs to
		for (size_t o = 0; o < std::size(impl->cellProcOffsets); o += 4)
		{
			int cell = CellIdx(impl, _mm_add_epi32(centerCell, *reinterpret_cast<const __m128i*>(impl->cellProcOffsets + o)));
			if (cell == -1)
				continue;
			for (int i = 0; i < impl->cellNumParticles[cell]; i++)
			{
				uint16_t b = impl->cellParticles[cell][i];
				if (b != p)
				{
					//Check particle b for proximity
					__m128 sep = _mm_sub_ps(particlePos, impl->particlePos[b]);
					float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8));
					if (dist2 < INFLUENCE_RADIUS * INFLUENCE_RADIUS)
					{
						impl->closeParticles[p][numClose++] = b;
						if (numClose == MAX_CLOSE) break;
					}
				}
			}
			if (numClose == MAX_CLOSE) break;
		}
		impl->numCloseParticles[p] = numClose;
	});
	
	//Changes gravity for particles.
	// This is done by running a DFS across the graph of close particles, starting at the changed particle.
	if (args.shouldChangeParticleGravity)
	{
		__m128 changePos = { args.changeGravityParticlePos.x, args.changeGravityParticlePos.y, args.changeGravityParticlePos.z, 0 };
		
		//Finds the particle closest to the change gravity particle position
		size_t changeGravityParticle = SIZE_MAX;
		float closestParticleDist2 = INFINITY;
		for (size_t i = 0; i < impl->numParticles; i++)
		{
			__m128 sep = _mm_sub_ps(changePos, impl->particlePos[i]);
			float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8));
			if (dist2 < closestParticleDist2)
			{
				closestParticleDist2 = dist2;
				changeGravityParticle = i;
			}
		}
		
		if (changeGravityParticle != SIZE_MAX)
		{
			std::vector<bool> seen(impl->numParticles, false);
			seen[changeGravityParticle] = true;
			std::vector<uint16_t> particlesStack;
			particlesStack.reserve(impl->numParticles);
			particlesStack.push_back(eg::UnsignedNarrow<uint16_t>(changeGravityParticle));
			while (!particlesStack.empty())
			{
				uint16_t cur = particlesStack.back();
				particlesStack.pop_back();
				
				if (!args.changeGravityParticleHighlightOnly)
				{
					impl->particleGravity[cur] = static_cast<uint8_t>(args.newGravity);
				}
				impl->particlePos[cur][3] = args.gameTime;
				
				for (uint16_t bI = 0; bI < impl->numCloseParticles[cur]; bI++)
				{
					uint16_t b = impl->closeParticles[cur][bI];
					if (!seen[b])
					{
						particlesStack.push_back(b);
						seen[b] = true;
					}
				}
			}
			
			if (!args.changeGravityParticleHighlightOnly)
			{
				impl->particleGravityVersion++;
			}
		}
	}
	
	//Computes number density
	for (size_t a = 0; a < impl->numParticles; a++)
	{
		for (uint16_t bI = 0; bI < impl->numCloseParticles[a]; bI++)
		{
			uint16_t b = impl->closeParticles[a][bI];
			if (b > a)
				continue;
			__m128 sep = _mm_sub_ps(impl->particlePos[a], impl->particlePos[b]);
			const float dist = glm::fastSqrt(_mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8)));
			const float q    = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2   = q * q;
			const float q3   = q2 * q;
			const float q4   = q2 * q2;
			impl->particleDensity[a].x += q3;
			impl->particleDensity[a].y += q4;
			impl->particleDensity[b].x += q3;
			impl->particleDensity[b].y += q4;
		}
	}
	
	const __m128 dt4 = { dt, dt, dt, 0 };
	
	//Accelerates particles
	std::for_each_n(std::execution::par_unseq, impl->particlePos, impl->numParticles, [&] (__m128& aPos)
	{
		uint32_t a = static_cast<uint32_t>(&aPos - impl->particlePos);
		
		//Initializes acceleration to the gravitational acceleration
		const float relativeDensity = (impl->particleDensity[a].x - AMBIENT_DENSITY) / impl->particleDensity[a].x;
		__m128 accel = _mm_mul_ps(_mm_set1_ps(relativeDensity),
			*reinterpret_cast<const __m128*>(gravities[impl->particleGravity[a]]));
		
		//Applies acceleration due to pressure
		for (uint16_t bI = 0; bI < impl->numCloseParticles[a]; bI++)
		{
			uint16_t b = impl->closeParticles[a][bI];
			__m128 sep = _mm_sub_ps(impl->particlePos[a], impl->particlePos[b]);
			float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8));
			
			//Randomly separates particles that are very close so that the pressure gradient won't be zero.
			if (dist2 < CORE_RADIUS * CORE_RADIUS || std::abs(sep[0]) < CORE_RADIUS ||
			    std::abs(sep[1]) < CORE_RADIUS || std::abs(sep[2]) < CORE_RADIUS)
			{
				std::uniform_real_distribution<float> offsetDist(-CORE_RADIUS / 2, CORE_RADIUS);
				alignas(16) float offset[4] = { offsetDist(impl->rng), offsetDist(impl->rng), offsetDist(impl->rng), 0 };
				sep = _mm_add_ps(sep, *reinterpret_cast<__m128*>(offset));
				dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8));
			}
			
			const float dist = std::sqrt(dist2);
			const float q    = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2   = q * q;
			const float q3   = q2 * q;
			
			const float densA     = impl->particleDensity[a].x;
			const float densB     = impl->particleDensity[b].x;
			const float nearDensA = impl->particleDensity[a].y;
			const float nearDensB = impl->particleDensity[b].y;
			const float pressure  = STIFFNESS * (densA + densB - 2.0f * TARGET_NUMBER_DENSITY);
			const float pressNear = STIFFNESS * NEAR_TO_FAR * ( nearDensA + nearDensB );
			const float accelScale = (pressure * q2 + pressNear * q3) / (dist + FLT_EPSILON);
			accel = _mm_add_ps(accel, _mm_mul_ps(sep, _mm_set1_ps(accelScale)));
		}
		
		//Applies acceleration to the particle's velocity
		impl->particleVel[a] = _mm_add_ps(impl->particleVel[a], _mm_mul_ps(accel, dt4));
	});
	
	//Velocity diffusion & collision
	std::for_each_n(std::execution::par_unseq, impl->particlePos, impl->numParticles, [&] (__m128& aPos)
	{
		uint32_t a = static_cast<uint32_t>(&aPos - impl->particlePos);
		__m128 vel = impl->particleVel[a];
		
		uint8_t particleGravityMask = (uint8_t)1 << impl->particleGravity[a];
		
		//Velocity diffusion
		for (uint16_t bI = 0; bI < impl->numCloseParticles[a]; bI++)
		{
			uint16_t b = impl->closeParticles[a][bI];
			__m128 velA = impl->particleVel[a];
			__m128 velB = impl->particleVel[b];
			
			__m128 sep     = _mm_sub_ps(impl->particlePos[a], impl->particlePos[b]);
			float invDist  = glm::fastInverseSqrt(_mm_cvtss_f32(_mm_dp_ps(sep, sep, DP_IMM8)));
			__m128 sepDir  = _mm_mul_ps(sep, _mm_set1_ps(invDist));
			__m128 velDiff = _mm_sub_ps(velA, velB);
			float velSep   = _mm_cvtss_f32(_mm_dp_ps(velDiff, sepDir, DP_IMM8));
			if (velSep < 0.0f)
			{
				const float infl         = std::max(1.0f - (1.0f / INFLUENCE_RADIUS) / invDist, 0.0f);
				const float velSepA      = _mm_cvtss_f32(_mm_dp_ps(velA, sepDir, DP_IMM8));
				const float velSepB      = _mm_cvtss_f32(_mm_dp_ps(velB, sepDir, DP_IMM8));
				const float velSepTarget = (velSepA + velSepB) * 0.5f;
				const float diffSepA     = velSepTarget - velSepA;
				const float changeSepA   = RADIAL_VISCOSITY_GAIN * diffSepA * infl;
				const __m128 changeA     = _mm_mul_ps(_mm_set1_ps(changeSepA), sepDir);
				vel = _mm_add_ps(vel, changeA);
			}
		}
		
		//Applies the velocity to the particle's position
		const float VEL_SCALE = 0.998f;
		const float MAX_MOVE = 0.2f;
		vel = _mm_mul_ps(vel, _mm_set1_ps(VEL_SCALE));
		__m128 move = _mm_mul_ps(vel, dt4);
		float moveDistSquared = _mm_cvtss_f32(_mm_dp_ps(move, move, DP_IMM8));
		if (moveDistSquared > MAX_MOVE * MAX_MOVE)
		{
			float scale = MAX_MOVE * glm::fastInverseSqrt(moveDistSquared);
			move = _mm_mul_ps(move, _mm_set1_ps(scale));
		}
		impl->particlePos2[a] = _mm_add_ps(impl->particlePos[a], move);
		
		__m128 halfM = _mm_set1_ps(0.5f);
		
		//Collision detection. This works by looping over all voxels close to the particle (within CHECK_RAD), then
		// checking all faces that face from a solid voxel to an air voxel.
		for (int tr = 0; tr < 5; tr++)
		{
			__m128i centerVx = _mm_and_si128(_mm_cvtps_epi32(_mm_floor_ps(impl->particlePos2[a])), and3Comp);
			EG_DEBUG_ASSERT(_mm_extract_epi32(centerVx, 3) == 0);
			
			float minDist = 0;
			__m128 displaceNormal = _mm_setzero_ps();
			
			auto CheckFace = [&] (__m128 planePoint, __m128 normal, __m128 tangent, __m128 bitangent,
				float tangentLen, float biTangentLen, float minDistC)
			{
				float distC = _mm_cvtss_f32(_mm_dp_ps(_mm_sub_ps(impl->particlePos2[a], planePoint), normal, DP_IMM8));
				float distE = distC - impl->particleRadius[a] * 0.5f;
				
				if (distC > minDistC && distE < minDist)
				{
					__m128 iPos = _mm_sub_ps(_mm_add_ps(impl->particlePos2[a], _mm_mul_ps(_mm_set1_ps(distC), normal)), planePoint);
					float iDotT = _mm_cvtss_f32(_mm_dp_ps(iPos, tangent, DP_IMM8));
					float iDotBT = _mm_cvtss_f32(_mm_dp_ps(iPos, bitangent, DP_IMM8));
					float iDotN = _mm_cvtss_f32(_mm_dp_ps(iPos, normal, DP_IMM8));
					
					//Checks that the intersection actually happened on the voxel's face
					if (std::abs(iDotT) <= tangentLen && std::abs(iDotBT) <= biTangentLen && std::abs(iDotN) < 0.8f)
					{
						minDist = distE - 0.001f;
						displaceNormal = normal;
					}
				}
			};
			
			for (const WaterBlocker& blocker : args.waterBlockers)
			{
				if (blocker.blockedGravities & particleGravityMask)
				{
					CheckFace(blocker.center, blocker.normal, blocker.tangent, blocker.biTangent,
					          blocker.tangentLen, blocker.biTangentLen, 0);
				}
			}
			
			alignas(16) int32_t offset[4] = { };
			constexpr int CHECK_RAD = 1;
			for (offset[2] = -CHECK_RAD; offset[2] <= CHECK_RAD; offset[2]++)
			{
				for (offset[1] = -CHECK_RAD; offset[1] <= CHECK_RAD; offset[1]++)
				{
					for (offset[0] = -CHECK_RAD; offset[0] <= CHECK_RAD; offset[0]++)
					{
						__m128i voxelCoordSolid = _mm_add_epi32(centerVx, *reinterpret_cast<__m128i*>(offset));
						
						//This voxel must be solid
						if (IsVoxelAir(impl, voxelCoordSolid))
							continue;
						
						for (size_t n = 0; n < std::size(voxelNormalsI); n++)
						{
							__m128i normal = *reinterpret_cast<const __m128i*>(voxelNormalsI[n]);
							
							//This voxel cannot be solid
							if (!IsVoxelAir(impl, _mm_add_epi32(voxelCoordSolid, normal)))
								continue;
							
							__m128 normalF = *reinterpret_cast<const __m128*>(voxelNormalsF[n]);
							
							//A point on the plane going through this face
							__m128 planePoint = _mm_add_ps(
								_mm_add_ps(_mm_cvtepi32_ps(voxelCoordSolid), halfM),
								_mm_mul_ps(normalF, halfM)
							);
							
							__m128i tangent = *reinterpret_cast<const __m128i*>(voxelTangentsI[n]);
							__m128i bitangent = *reinterpret_cast<const __m128i*>(voxelBitangentsI[n]);
							float tangentLen =
								!IsVoxelAir(impl, _mm_add_epi32(voxelCoordSolid, tangent)) &&
								!IsVoxelAir(impl, _mm_sub_epi32(voxelCoordSolid, tangent)) ? 0.6f : 0.5f;
							float bitangentLen =
								!IsVoxelAir(impl, _mm_add_epi32(voxelCoordSolid, bitangent)) &&
								!IsVoxelAir(impl, _mm_sub_epi32(voxelCoordSolid, bitangent)) ? 0.6f : 0.5f;
							
							CheckFace(planePoint, normalF,
				                      *reinterpret_cast<const __m128*>(voxelTangentsF[n]),
			                          *reinterpret_cast<const __m128*>(voxelBitangentsF[n]),
			                          tangentLen, bitangentLen, -INFINITY);
						}
					}
				}
			}
			
			//Applies an impulse to the velocity
			if (minDist < 0)
			{
				float vDotDisplace = _mm_cvtss_f32(_mm_dp_ps(vel, displaceNormal, DP_IMM8)) * IMPACT_COEFFICIENT;
				__m128 impulse = _mm_mul_ps(displaceNormal, _mm_set1_ps(vDotDisplace));
				vel = _mm_sub_ps(vel, impulse);
			}
			
			//Applies collision correction
			const __m128 minDist4 = { minDist, minDist, minDist, 0 };
			impl->particlePos2[a] = _mm_sub_ps(impl->particlePos2[a], _mm_mul_ps(displaceNormal, minDist4));
		}
		
		impl->particleVel2[a] = vel;
	});
}

#endif
