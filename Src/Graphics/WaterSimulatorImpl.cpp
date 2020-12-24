#include <cstdlib>
#include <execution>
#include <glm/gtx/fast_square_root.hpp>

#include "WaterSimulatorImpl.hpp"

#ifdef _WIN32
inline static void* aligned_alloc(size_t alignment, size_t size)
{
	return _aligned_malloc(size, alignment);
}
#endif

//Maximum number of particles per partition grid cell
static constexpr uint32_t MAX_PER_PART_CELL = 512;

//Maximum number of close particles
static constexpr uint32_t MAX_CLOSE = 512;

static constexpr float ELASTICITY = 0.2;
static constexpr float IMPACT_COEFFICIENT = 1.0 + ELASTICITY;
static constexpr float MIN_PARTICLE_RADIUS = 0.1;
static constexpr float MAX_PARTICLE_RADIUS = 0.4;
static constexpr float INFLUENCE_RADIUS = 0.6;
static constexpr float CORE_RADIUS = 0.001;
static constexpr float RADIAL_VISCOSITY_GAIN = 0.25;
static constexpr float TARGET_NUMBER_DENSITY = 2.5;
static constexpr float STIFFNESS = 20.0;
static constexpr float NEAR_TO_FAR = 2.0;
static constexpr float AMBIENT_DENSITY = 0.5;
static constexpr int CELL_GROUP_SIZE = 4;

static std::uniform_real_distribution<float> radiusDist(MIN_PARTICLE_RADIUS, MAX_PARTICLE_RADIUS);

struct WaterSimulatorImpl
{
	alignas(16) int32_t cellProcOffsets[4 * 3 * 3 * 3];
	
	std::mt19937 rng;
	
	uint32_t numParticles;
	uint32_t allocatedParticles;
	
	//Memory for particle data
	void* dataMemory;
	
	//SoA particle data, points into dataMemory
	__m128* particlePos;
	__m128* particlePos2;
	__m128* particleVel;
	__m128* particleVel2;
	float* particleRadius;
	uint8_t* particleGravity;
	glm::vec2* particleDensity;
	
	//Stores which cell each particle belongs to
	__m128i* particleCells;
	
	//For each partition cell, stores how many particles belong to that cell.
	std::vector<uint16_t> cellNumParticles;
	
	//For each partition cell, stores a list of particle indices that belong to that cell.
	std::vector<std::array<uint16_t, MAX_PER_PART_CELL>> cellParticles;
	
	__m128i worldMin;
	glm::ivec3 worldSize;
	uint8_t* isVoxelAir;
	
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
	
	for (int i = 0; i < 3; i++)
	{
		reinterpret_cast<int32_t*>(&impl->worldMin)[i] = args.minBounds[i];
	}
	
	impl->worldSize = args.maxBounds - args.minBounds;
	impl->isVoxelAir = args.isAirBuffer;
	impl->numParticles = args.numParticles;
	impl->allocatedParticles = args.numParticles + args.extraParticles;
	
	constexpr int GRID_CELLS_MARGIN = 5;
	impl->partGridMin = glm::ivec3(glm::floor(glm::vec3(args.minBounds) / INFLUENCE_RADIUS)) - GRID_CELLS_MARGIN;
	glm::ivec3 partGridMax = glm::ivec3(glm::ceil(glm::vec3(args.maxBounds) / INFLUENCE_RADIUS)) + GRID_CELLS_MARGIN;
	impl->partGridNumCellGroups = ((partGridMax - impl->partGridMin) + (CELL_GROUP_SIZE - 1)) / CELL_GROUP_SIZE;
	impl->partGridNumCells = impl->partGridNumCellGroups * CELL_GROUP_SIZE;
	impl->cellNumParticles.resize(impl->partGridNumCells.x * impl->partGridNumCells.y * impl->partGridNumCells.z);
	impl->cellParticles.resize(impl->cellNumParticles.size());
	
	//Allocates memory for particle data
	size_t memoryBytes = impl->allocatedParticles * (sizeof(__m128) * 5 + sizeof(uint8_t) + sizeof(float) + sizeof(glm::vec2));
	impl->dataMemory = aligned_alloc(alignof(__m128), memoryBytes);
	
	//Sets up SoA particle data
	impl->particlePos             = reinterpret_cast<__m128*>(impl->dataMemory);
	impl->particlePos2            = impl->particlePos + impl->allocatedParticles;
	impl->particleVel             = impl->particlePos2 + impl->allocatedParticles;
	impl->particleVel2            = impl->particleVel + impl->allocatedParticles;
	impl->particleCells           = reinterpret_cast<__m128i*>(impl->particleVel2 + impl->allocatedParticles);
	impl->particleGravity         = reinterpret_cast<uint8_t*>(impl->particleCells + impl->allocatedParticles);
	impl->particleRadius          = reinterpret_cast<float*>(impl->particleGravity + impl->allocatedParticles);
	impl->particleDensity         = reinterpret_cast<glm::vec2*>(impl->particleRadius + impl->allocatedParticles);
	if (reinterpret_cast<char*>(impl->particleDensity + impl->allocatedParticles) != (char*)impl->dataMemory + memoryBytes)
		std::abort();
	
	//Generates random particle radii
	for (uint32_t i = 0; i < impl->numParticles; i++)
		impl->particleRadius[i] = radiusDist(impl->rng);
	
	std::fill_n(impl->particleGravity, impl->numParticles, (uint8_t)Dir::NegY);
	std::memset(impl->particleVel, 0, sizeof(float) * 4 * impl->numParticles);
	std::memset(impl->particlePos, 0, sizeof(float) * 4 * impl->numParticles);
	
	//Copies particle positions to m_particlePos
	for (size_t i = 0; i < args.numParticles; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			reinterpret_cast<float*>(impl->particlePos + i)[j] = args.particlePositions[i * 3 + j];
		}
	}
	
	impl->closeParticles.resize(impl->allocatedParticles);
	impl->numCloseParticles.resize(impl->allocatedParticles);
	
	return impl;
}

void WSI_Delete(WaterSimulatorImpl* impl)
{
	std::free(impl->dataMemory);
	std::free(impl->isVoxelAir);
	delete impl;
}

uint32_t WSI_GetPositions(WaterSimulatorImpl* impl, void* destination)
{
	std::memcpy(destination, impl->particlePos, impl->numParticles * sizeof(float) * 4);
	return impl->numParticles;
}

void WSI_SwapBuffers(WaterSimulatorImpl* impl)
{
	std::swap(impl->particleVel2, impl->particleVel);
	std::swap(impl->particlePos2, impl->particlePos);
}

inline int CellIdx(WaterSimulatorImpl* impl, __m128i coord4)
{
	const int32_t* coord = reinterpret_cast<const int32_t*>(&coord4);
	
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
	int32_t* rVoxelI = reinterpret_cast<int32_t*>(&rVoxel);
	
	if (rVoxelI[0] < 0 || rVoxelI[1] < 0 || rVoxelI[2] < 0 ||
	    rVoxelI[0] >= impl->worldSize.x || rVoxelI[1] >= impl->worldSize.y || rVoxelI[2] >= impl->worldSize.z)
	{
		return false;
	}
	size_t idx = rVoxelI[0] + rVoxelI[1] * impl->worldSize.x + rVoxelI[2] * impl->worldSize.x * impl->worldSize.y;
	return impl->isVoxelAir[idx / 8] & (1 << (idx % 8));
}

int WSI_Query(WaterSimulatorImpl* impl, const eg::AABB& aabb, glm::vec3& waterVelocity, glm::vec3& buoyancy)
{
	int numIntersecting = 0;
	for (uint32_t p = 0; p < impl->numParticles; p++)
	{
		glm::vec3 pos = *reinterpret_cast<glm::vec3*>(&impl->particlePos2[p]);
		eg::AABB particleAABB(pos - impl->particleRadius[p], pos + impl->particleRadius[p]);
		
		if (aabb.Intersects(particleAABB))
		{
			numIntersecting++;
			waterVelocity += *reinterpret_cast<glm::vec3*>(&impl->particleVel2[p]);
			buoyancy -= DirectionVector((Dir)impl->particleGravity[p]);
		}
	}
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

void WSI_Simulate(WaterSimulatorImpl* impl, const WSISimulateArgs& args)
{
	float dt = args.dt;
	
	std::memset(impl->cellNumParticles.data(), 0, impl->cellNumParticles.size() * sizeof(uint16_t));
	
	__m128 gridCell4 = _mm_load1_ps(&INFLUENCE_RADIUS);
	alignas(16) const int32_t gridCellsMin4[] = { impl->partGridMin.x, impl->partGridMin.y, impl->partGridMin.z, 0 };
	
	//Partitions particles into grid cells so that detecting close particles will be faster.
	for (uint32_t i = 0; i < impl->numParticles; i++)
	{
		impl->particleDensity[i] = glm::vec2(1.0f);
		
		//Computes the cell to place this particle into based on the floor of the particle's position.
		impl->particleCells[i] = _mm_sub_epi32(_mm_cvtps_epi32(_mm_floor_ps(_mm_div_ps(impl->particlePos[i], gridCell4))),
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
			for (int i = 0; i < impl->cellNumParticles[cell] && numClose < MAX_CLOSE; i++)
			{
				uint16_t b = impl->cellParticles[cell][i];
				if (b != p)
				{
					//Check particle b for proximity
					__m128 sep = _mm_sub_ps(particlePos, impl->particlePos[b]);
					float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF));
					if (dist2 < INFLUENCE_RADIUS * INFLUENCE_RADIUS)
					{
						impl->closeParticles[p][numClose++] = b;
					}
				}
			}
		}
		impl->numCloseParticles[p] = numClose;
	});
	
	//Changes gravity for particles.
	// This is done by running a DFS across the graph of close particles, starting at the changed particle.
	if (args.changeGravityParticle != -1)
	{
		std::vector<bool> seen(impl->numParticles, false);
		seen[args.changeGravityParticle] = true;
		std::vector<int> particlesStack;
		particlesStack.reserve(impl->numParticles);
		particlesStack.push_back(args.changeGravityParticle);
		while (!particlesStack.empty())
		{
			int cur = particlesStack.back();
			particlesStack.pop_back();
			
			impl->particleGravity[cur] = (uint8_t)args.newGravity;
			
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
			const float dist = glm::fastSqrt(_mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF)));
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
	
	const __m128 dt4 = _mm_load1_ps(&dt);
	
	//Accelerates particles
	std::for_each_n(std::execution::par_unseq, impl->particlePos, impl->numParticles, [&] (__m128& aPos)
	{
		uint32_t a = &aPos - impl->particlePos;
		
		//Initializes acceleration to the gravitational acceleration
		const float relativeDensity = (impl->particleDensity[a].x - AMBIENT_DENSITY) / impl->particleDensity[a].x;
		__m128 accel = _mm_mul_ps(_mm_load1_ps(&relativeDensity),
			*reinterpret_cast<const __m128*>(gravities[impl->particleGravity[a]]));
		
		//Applies acceleration due to pressure
		for (uint16_t bI = 0; bI < impl->numCloseParticles[a]; bI++)
		{
			uint16_t b = impl->closeParticles[a][bI];
			__m128 sep = _mm_sub_ps(impl->particlePos[a], impl->particlePos[b]);
			float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF));
			
			//Randomly separates particles that are very close so that the pressure gradient won't be zero.
			if (dist2 < CORE_RADIUS * CORE_RADIUS || std::abs(sep[0]) < CORE_RADIUS ||
			    std::abs(sep[1]) < CORE_RADIUS || std::abs(sep[2]) < CORE_RADIUS)
			{
				std::uniform_real_distribution<float> offsetDist(-CORE_RADIUS / 2, CORE_RADIUS);
				alignas(16) float offset[4] = { offsetDist(impl->rng), offsetDist(impl->rng), offsetDist(impl->rng), 0 };
				sep = _mm_add_ps(sep, *reinterpret_cast<__m128*>(offset));
				dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF));
			}
			
			const float dist    = std::sqrt(dist2);
			const float q       = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2      = q * q;
			const float q3      = q2 * q;
			
			const float densA     = impl->particleDensity[a].x;
			const float densB     = impl->particleDensity[b].x;
			const float nearDensA = impl->particleDensity[a].y;
			const float nearDensB = impl->particleDensity[b].y;
			const float pressure  = STIFFNESS * (densA + densB - 2.0f * TARGET_NUMBER_DENSITY);
			const float pressNear = STIFFNESS * NEAR_TO_FAR * ( nearDensA + nearDensB );
			const float accelScale = (pressure * q2 + pressNear * q3) / (dist + FLT_EPSILON);
			accel = _mm_add_ps(accel, _mm_mul_ps(sep, _mm_load1_ps(&accelScale)));
		}
		
		//Applies acceleration to the particle's velocity
		impl->particleVel[a] = _mm_add_ps(impl->particleVel[a], _mm_mul_ps(accel, dt4));
	});
	
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
	
	//Velocity diffusion & collision
	std::for_each_n(std::execution::par_unseq, impl->particlePos, impl->numParticles, [&] (__m128& aPos)
	{
		uint32_t a = &aPos - impl->particlePos;
		__m128 vel = impl->particleVel[a];
		
		uint8_t particleGravityMask = (uint8_t)1 << impl->particleGravity[a];
		
		//Velocity diffusion
		for (uint16_t bI = 0; bI < impl->numCloseParticles[a]; bI++)
		{
			uint16_t b = impl->closeParticles[a][bI];
			__m128 velA = impl->particleVel[a];
			__m128 velB = impl->particleVel[b];
			
			__m128 sep     = _mm_sub_ps(impl->particlePos[a], impl->particlePos[b]);
			float dist     = glm::fastSqrt(_mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF)));
			__m128 sepDir  = _mm_div_ps(sep, _mm_load1_ps(&dist));
			__m128 velDiff = _mm_sub_ps(velA, velB);
			float velSep   = _mm_cvtss_f32(_mm_dp_ps(velDiff, sepDir, 0xFF));
			if (velSep < 0.0f)
			{
				const float infl         = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
				const float velSepA      = _mm_cvtss_f32(_mm_dp_ps(velA, sepDir, 0xFF));
				const float velSepB      = _mm_cvtss_f32(_mm_dp_ps(velB, sepDir, 0xFF));
				const float velSepTarget = (velSepA + velSepB) * 0.5f;
				const float diffSepA     = velSepTarget - velSepA;
				const float changeSepA   = RADIAL_VISCOSITY_GAIN * diffSepA * infl;
				const __m128 changeA     = _mm_mul_ps(_mm_load1_ps(&changeSepA), sepDir);
				vel = _mm_add_ps(vel, changeA);
			}
		}
		
		//Applies the velocity to the particle's position
		const float VEL_SCALE = 0.998f;
		const float MAX_MOVE = 0.2f;
		vel = _mm_mul_ps(vel, _mm_load1_ps(&VEL_SCALE));
		__m128 move = _mm_mul_ps(vel, dt4);
		float moveDistSquared = _mm_cvtss_f32(_mm_dp_ps(move, move, 0xFF));
		if (moveDistSquared > MAX_MOVE * MAX_MOVE)
		{
			float scale = MAX_MOVE * glm::fastInverseSqrt(moveDistSquared);
			move = _mm_mul_ps(move, _mm_load1_ps(&scale));
		}
		impl->particlePos2[a] = _mm_add_ps(impl->particlePos[a], move);
		
		alignas(16) static const float half[] = { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128& halfM = *reinterpret_cast<const __m128*>(half);
		
		//Collision detection. This works by looping over all voxels close to the particle (within CHECK_RAD), then
		// checking all faces that face from a solid voxel to an air voxel.
		for (int tr = 0; tr < 5; tr++)
		{
			__m128i centerVx = _mm_cvtps_epi32(_mm_floor_ps(impl->particlePos2[a]));
			
			float minDist = 0;
			__m128 displaceNormal;
			
			auto CheckFace = [&] (__m128 planePoint, __m128 normal, __m128 tangent, __m128 bitangent,
				float tangentLen, float biTangentLen, float minDistC)
			{
				float distC = _mm_cvtss_f32(_mm_dp_ps(_mm_sub_ps(impl->particlePos2[a], planePoint), normal, 0xFF));
				float distE = distC - impl->particleRadius[a] * 0.5f;
				
				if (distC > minDistC && distE < minDist)
				{
					__m128 iPos = _mm_sub_ps(_mm_add_ps(impl->particlePos2[a], _mm_mul_ps(_mm_load1_ps(&distC), normal)), planePoint);
					float iDotT = _mm_cvtss_f32(_mm_dp_ps(iPos, tangent, 0xFF));
					float iDotBT = _mm_cvtss_f32(_mm_dp_ps(iPos, bitangent, 0xFF));
					float iDotN = _mm_cvtss_f32(_mm_dp_ps(iPos, normal, 0xFF));
					
					//Checks that the intersection actually happened on the voxel's face
					if (std::abs(iDotT) <= tangentLen && std::abs(iDotBT) <= biTangentLen && std::abs(iDotN) < 0.8f)
					{
						minDist = distE - 0.001f;
						displaceNormal = normal;
					}
				}
			};
			
			for (size_t i = 0; i < args.numWaterBlockers; i++)
			{
				if (!(args.waterBlockers[i].blockedGravities & particleGravityMask))
					continue;
				
				CheckFace(args.waterBlockers[i].center, args.waterBlockers[i].normal, args.waterBlockers[i].tangent,
				          args.waterBlockers[i].biTangent, args.waterBlockers[i].tangentLen,
				          args.waterBlockers[i].biTangentLen, 0);
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
				float vDotDisplace = _mm_cvtss_f32(_mm_dp_ps(vel, displaceNormal, 0xFF)) * IMPACT_COEFFICIENT;
				__m128 impulse = _mm_mul_ps(displaceNormal, _mm_load1_ps(&vDotDisplace));
				vel = _mm_sub_ps(vel, impulse);
			}
			
			//Applies collision correction
			impl->particlePos2[a] = _mm_sub_ps(impl->particlePos2[a], _mm_mul_ps(displaceNormal, _mm_load1_ps(&minDist)));
		}
		
		impl->particleVel2[a] = vel;
	});
}
