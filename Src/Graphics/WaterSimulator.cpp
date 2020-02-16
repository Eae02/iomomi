#include "WaterSimulator.hpp"
#include "../World/World.hpp"
#include "../World/Player.hpp"
#include "../World/Entities/EntTypes/WaterPlaneEnt.hpp"
#include "../Vec3Compare.hpp"

#include <cstdlib>
#include <execution>

#ifdef _WIN32
inline static void* aligned_alloc(size_t alignment, size_t size)
{
	return _aligned_malloc(size, alignment);
}
#endif

static constexpr float ELASTICITY = 0.2;
static constexpr float IMPACT_COEFFICIENT = 1.0 + ELASTICITY;
static constexpr float MIN_PARTICLE_RADIUS = 0.2;
static constexpr float MAX_PARTICLE_RADIUS = 0.3;
static constexpr float INFLUENCE_RADIUS = 0.6;
static constexpr float CORE_RADIUS = 0.001;
static constexpr float RADIAL_VISCOSITY_GAIN = 0.25;
static constexpr float TARGET_NUMBER_DENSITY = 2.5;
static constexpr float STIFFNESS = 20.0;
static constexpr float NEAR_TO_FAR = 2.0;
static constexpr float SPEED_LIMIT = 30.0;
static constexpr float AMBIENT_DENSITY = 0.5;
static constexpr int CELL_GROUP_SIZE = 4;

WaterSimulator::WaterSimulator()
{
	for (int z = 0; z < 3; z++)
	{
		for (int y = 0; y < 3; y++)
		{
			for (int x = 0; x < 3; x++)
			{
				int baseIdx = x + y * 3 + z * 9;
				m_cellProcOffsets[baseIdx * 4 + 0] = x - 1;
				m_cellProcOffsets[baseIdx * 4 + 1] = y - 1;
				m_cellProcOffsets[baseIdx * 4 + 2] = z - 1;
				m_cellProcOffsets[baseIdx * 4 + 3] = 0;
			}
		}
	}
}

void WaterSimulator::Init(World& world)
{
	Stop();
	
	for (int i = 0; i < 3; i++)
	{
		reinterpret_cast<int32_t*>(&m_worldMin)[i] = world.GetBoundsMin()[i];
	}
	m_worldSize = world.GetBoundsMax() - world.GetBoundsMin();
	m_world = &world;
	
	//Copies voxel air data to m_isVoxelAir
	m_isVoxelAir.resize(m_worldSize.x * m_worldSize.y * m_worldSize.z);
	for (int z = 0; z < m_worldSize.z; z++)
	{
		for (int y = 0; y < m_worldSize.y; y++)
		{
			for (int x = 0; x < m_worldSize.x; x++)
			{
				m_isVoxelAir[x + y * m_worldSize.x + z * m_worldSize.x * m_worldSize.y] = world.IsAir(glm::ivec3(x, y, z) + world.GetBoundsMin());
			}
		}
	}
	
	constexpr int GRID_CELLS_MARGIN = 5;
	m_partGridMin = glm::ivec3(glm::floor(glm::vec3(world.GetBoundsMin()) / INFLUENCE_RADIUS)) - GRID_CELLS_MARGIN;
	glm::ivec3 partGridMax = glm::ivec3(glm::ceil(glm::vec3(world.GetBoundsMax()) / INFLUENCE_RADIUS)) + GRID_CELLS_MARGIN;
	m_partGridNumCellGroups = ((partGridMax - m_partGridMin) + (CELL_GROUP_SIZE - 1)) / CELL_GROUP_SIZE;
	m_partGridNumCells = m_partGridNumCellGroups * CELL_GROUP_SIZE;
	m_cellNumParticles.resize(m_partGridNumCells.x * m_partGridNumCells.y * m_partGridNumCells.z);
	m_cellParticles.resize(m_cellNumParticles.size());
	
	//Generates a vector of all underwater cells
	std::vector<glm::ivec3> underwaterCells;
	world.entManager.ForEachOfType<WaterPlaneEnt>([&] (WaterPlaneEnt& waterPlaneEntity)
	{
		//Add all cells from this liquid plane to the vector
		waterPlaneEntity.m_liquidPlane.MaybeUpdate(waterPlaneEntity, world);
		for (glm::ivec3 cell : waterPlaneEntity.m_liquidPlane.UnderwaterCells())
		{
			underwaterCells.push_back(cell);
		}
	});
	if (underwaterCells.empty())
	{
		m_numParticles = 0;
		return;
	}
	
	//Removes duplicates from the list of underwater cells
	std::sort(underwaterCells.begin(), underwaterCells.end(), IVec3Compare());
	size_t lastCell = std::unique(underwaterCells.begin(), underwaterCells.end()) - underwaterCells.begin();
	
	//The number of particles to generate per underwater cell
	const glm::ivec3 GEN_PER_VOXEL(3, 4, 3);
	
	std::uniform_real_distribution<float> offsetDist(0.3f, 0.7f);
	
	//Generates particles
	std::vector<glm::vec3> positions;
	for (size_t i = 0; i < lastCell; i++)
	{
		for (int x = 0; x < GEN_PER_VOXEL.x; x++)
		{
			for (int y = 0; y < GEN_PER_VOXEL.y; y++)
			{
				for (int z = 0; z < GEN_PER_VOXEL.z; z++)
				{
					positions.push_back(
						glm::vec3(underwaterCells[i]) +
						glm::vec3(x + offsetDist(m_rng), y + offsetDist(m_rng), z + offsetDist(m_rng)) / glm::vec3(GEN_PER_VOXEL)
					);
				}
			}
		}
	}
	
	m_numParticles = positions.size();
	if (m_numParticles == 0)
		return;
	
	//Allocates memory for particle data
	size_t memoryBytes = positions.size() * (sizeof(__m128) * 6 + sizeof(uint8_t) + sizeof(float) + sizeof(glm::vec2));
	m_memory.reset(aligned_alloc(alignof(__m128), memoryBytes));
	
	//Sets up SoA particle data
	m_particlePos     = reinterpret_cast<__m128*>(m_memory.get());
	m_particlePos2    = m_particlePos + m_numParticles;
	m_particleVel     = m_particlePos2 + m_numParticles;
	m_particleVel2    = m_particleVel + m_numParticles;
	m_particlePosMT   = reinterpret_cast<glm::vec4*>(m_particleVel2 + m_numParticles);
	m_particleCells   = reinterpret_cast<__m128i*>(m_particlePosMT + m_numParticles);
	m_particleGravity = reinterpret_cast<uint8_t*>(m_particleCells + m_numParticles);
	m_particleRadius = reinterpret_cast<float*>(m_particleGravity + m_numParticles);
	m_particleDensity = reinterpret_cast<glm::vec2*>(m_particleRadius + m_numParticles);
	if (reinterpret_cast<char*>(m_particleDensity + m_numParticles) != reinterpret_cast<char*>(m_memory.get()) + memoryBytes)
		std::abort();
	
	//Generates random particle radii
	std::uniform_real_distribution<float> radiusDist(MIN_PARTICLE_RADIUS, MAX_PARTICLE_RADIUS);
	for (uint32_t i = 0; i < m_numParticles; i++)
		m_particleRadius[i] = radiusDist(m_rng);
	
	std::fill_n(m_particleGravity, m_numParticles, (uint8_t)Dir::NegY);
	std::memset(m_particleVel, 0, sizeof(float) * 4 * positions.size());
	std::memset(m_particlePos, 0, sizeof(float) * 4 * positions.size());
	
	//Copies particle positions to m_particlePos
	for (size_t i = 0; i < positions.size(); i++)
	{
		for (int j = 0; j < 3; j++)
		{
			reinterpret_cast<float*>(m_particlePos + i)[j] = positions[i][j];
		}
	}
	
	m_closeParticles.resize(positions.size());
	m_numCloseParticles.resize(positions.size());
	
	m_positionsBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer,
	                               sizeof(float) * 4 * m_numParticles, nullptr);
	
	m_run = true;
	m_thread = std::thread(&WaterSimulator::ThreadTarget, this);
}

void WaterSimulator::Stop()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_run)
			return;
		m_run = false;
	}
	m_thread.join();
	m_thread = { };
	m_memory.reset();
	m_positionsBuffer = { };
}

int WaterSimulator::CellIdx(__m128i coord4)
{
	const int32_t* coord = reinterpret_cast<const int32_t*>(&coord4);
	
	if (coord[0] < 0 || coord[1] < 0 || coord[2] < 0 ||
	    coord[0] >= m_partGridNumCells.x || coord[1] >= m_partGridNumCells.y || coord[2] >= m_partGridNumCells.z)
	{
		return -1;
	}
	
	glm::ivec3 group = glm::ivec3(coord[0], coord[1], coord[2]) / CELL_GROUP_SIZE;
	glm::ivec3 local = glm::ivec3(coord[0], coord[1], coord[2]) - group * CELL_GROUP_SIZE;
	int groupIdx = group.x + group.y * m_partGridNumCellGroups.x + group.z * m_partGridNumCellGroups.x * m_partGridNumCellGroups.y;
	
	return groupIdx * CELL_GROUP_SIZE * CELL_GROUP_SIZE * CELL_GROUP_SIZE +
	       local.x + local.y * CELL_GROUP_SIZE + local.z * CELL_GROUP_SIZE * CELL_GROUP_SIZE;
}

bool WaterSimulator::IsVoxelAir(__m128i voxel) const
{
	__m128i rVoxel = _mm_sub_epi32(voxel, m_worldMin);
	int32_t* rVoxelI = reinterpret_cast<int32_t*>(&rVoxel);
	
	if (rVoxelI[0] < 0 || rVoxelI[1] < 0 || rVoxelI[2] < 0 ||
	    rVoxelI[0] >= m_worldSize[0] || rVoxelI[1] >= m_worldSize[1] || rVoxelI[2] >= m_worldSize[2])
	{
		return false;
	}
	size_t idx = rVoxelI[0] + rVoxelI[1] * m_worldSize.x + rVoxelI[2] * m_worldSize.x * m_worldSize.y;
	return m_isVoxelAir[idx];
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

void WaterSimulator::Step(float dt, const eg::AABB& playerAABB, int changeGravityParticle, Dir newGravity)
{
	std::memset(m_cellNumParticles.data(), 0, m_cellNumParticles.size() * sizeof(uint16_t));
	
	__m128 gridCell4 = _mm_load1_ps(&INFLUENCE_RADIUS);
	alignas(16) const int32_t gridCellsMin4[] = { m_partGridMin.x, m_partGridMin.y, m_partGridMin.z, 0 };
	
	//Partitions particles into grid cells so that detecting close particles will be faster.
	for (uint32_t i = 0; i < m_numParticles; i++)
	{
		m_particleDensity[i] = glm::vec2(1.0f);
		
		//Computes the cell to place this particle into based on the floor of the particle's position.
		m_particleCells[i] = _mm_sub_epi32(_mm_cvtps_epi32(_mm_floor_ps(_mm_div_ps(m_particlePos[i], gridCell4))),
			*reinterpret_cast<const __m128i*>(gridCellsMin4));
		
		//Adds the particle to the cell
		int cell = CellIdx(m_particleCells[i]);
		if (cell != -1 && m_cellNumParticles[cell] < MAX_PER_PART_CELL)
		{
			m_cellParticles[cell][m_cellNumParticles[cell]++] = i;
		}
	}
	
	//Detects close particles by scanning the surrounding grid cells for each particle.
	m_closeParticles.clear();
	std::for_each_n(std::execution::par_unseq, m_particlePos, m_numParticles, [&] (__m128& particlePos)
	{
		uint32_t p = &particlePos - m_particlePos;
		__m128i centerCell = m_particleCells[p];
		uint32_t numClose = 0;
		
		//Loop through neighboring cells to the one the current particle belongs to
		for (size_t o = 0; o < std::size(m_cellProcOffsets); o += 4)
		{
			int cell = CellIdx(_mm_add_epi32(centerCell, *reinterpret_cast<const __m128i*>(m_cellProcOffsets + o)));
			if (cell == -1)
				continue;
			for (int i = 0; i < m_cellNumParticles[cell] && numClose < MAX_CLOSE; i++)
			{
				uint16_t b = m_cellParticles[cell][i];
				if (b != p)
				{
					//Check particle b for proximity
					__m128 sep = _mm_sub_ps(particlePos, m_particlePos[b]);
					float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF));
					if (dist2 < INFLUENCE_RADIUS * INFLUENCE_RADIUS)
					{
						m_closeParticles[p][numClose++] = b;
					}
				}
			}
		}
		m_numCloseParticles[p] = numClose;
	});
	
	//Changes gravity for particles.
	// This is done by running a DFS across the graph of close particles, starting at the changed particle.
	if (changeGravityParticle != -1)
	{
		std::vector<bool> seen(m_numParticles, false);
		seen[changeGravityParticle] = true;
		std::vector<int> particlesStack;
		particlesStack.reserve(m_numParticles);
		particlesStack.push_back(changeGravityParticle);
		while (!particlesStack.empty())
		{
			int cur = particlesStack.back();
			particlesStack.pop_back();
			
			m_particleGravity[cur] = (uint8_t)newGravity;
			
			for (uint16_t bI = 0; bI < m_numCloseParticles[cur]; bI++)
			{
				uint16_t b = m_closeParticles[cur][bI];
				if (!seen[b])
				{
					particlesStack.push_back(b);
					seen[b] = true;
				}
			}
		}
	}
	
	//Computes number density
	for (size_t a = 0; a < m_numParticles; a++)
	{
		for (uint16_t bI = 0; bI < m_numCloseParticles[a]; bI++)
		{
			uint16_t b = m_closeParticles[a][bI];
			if (b > a)
				continue;
			__m128 sep = _mm_sub_ps(m_particlePos[a], m_particlePos[b]);
			const float dist = std::sqrt(_mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF)));
			const float q    = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2   = q * q;
			const float q3   = q2 * q;
			const float q4   = q2 * q2;
			m_particleDensity[a].x += q3;
			m_particleDensity[a].y += q4;
			m_particleDensity[b].x += q3;
			m_particleDensity[b].y += q4;
		}
	}
	
	const __m128 dt4 = _mm_load1_ps(&dt);
	
	//Accelerates particles
	std::for_each_n(std::execution::par_unseq, m_particlePos, m_numParticles, [&] (__m128& aPos)
	{
		uint32_t a = &aPos - m_particlePos;
		
		//Initializes acceleration to the gravitational acceleration
		const float relativeDensity = (m_particleDensity[a].x - AMBIENT_DENSITY) / m_particleDensity[a].x;
		__m128 accel = _mm_mul_ps(_mm_load1_ps(&relativeDensity),
			*reinterpret_cast<const __m128*>(gravities[m_particleGravity[a]]));
		
		//Applies acceleration due to pressure
		for (uint16_t bI = 0; bI < m_numCloseParticles[a]; bI++)
		{
			uint16_t b = m_closeParticles[a][bI];
			__m128 sep = _mm_sub_ps(m_particlePos[a], m_particlePos[b]);
			float dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF));
			
			//Randomly seperates particles that are very close so that the pressure gradient won't be zero.
			if (dist2 < CORE_RADIUS * CORE_RADIUS || std::abs(sep[0]) < CORE_RADIUS ||
			    std::abs(sep[1]) < CORE_RADIUS || std::abs(sep[2]) < CORE_RADIUS)
			{
				std::uniform_real_distribution<float> offsetDist(-CORE_RADIUS / 2, CORE_RADIUS);
				alignas(16) float offset[4] = { offsetDist(m_rng), offsetDist(m_rng), offsetDist(m_rng), 0 };
				sep = _mm_add_ps(sep, *reinterpret_cast<__m128*>(offset));
				dist2 = _mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF));
			}
			
			const float dist    = std::sqrt(dist2);
			const float q       = std::max(1.0f - dist / INFLUENCE_RADIUS, 0.0f);
			const float q2      = q * q;
			const float q3      = q2 * q;
			
			const float densA     = m_particleDensity[a].x;
			const float densB     = m_particleDensity[b].x;
			const float nearDensA = m_particleDensity[a].y;
			const float nearDensB = m_particleDensity[b].y;
			const float pressure  = STIFFNESS * ( densA + densB - 2.0f * TARGET_NUMBER_DENSITY);
			const float pressNear = STIFFNESS * NEAR_TO_FAR * ( nearDensA + nearDensB );
			const float accelScale = (pressure * q2 + pressNear * q3) / (dist + FLT_EPSILON);
			accel = _mm_add_ps(accel, _mm_mul_ps(sep, _mm_load1_ps(&accelScale)));
		}
		
		//Applies acceleration to the particle's velocity
		m_particleVel[a] = _mm_add_ps(m_particleVel[a], _mm_mul_ps(accel, dt4));
	});
	
	alignas(16) const int32_t voxelNormals[][4] =
	{
		{ -1, 0, 0, 0}, { 1, 0, 0, 0 },
		{ 0, -1, 0, 0}, { 0, 1, 0, 0 },
		{ 0, 0, -1, 0}, { 0, 0, 1, 0 }
	};
	
	alignas(16) const float voxelTangents[][4] =
	{
		{ 0, 0, 1, 0 }, { 0, 0, 1, 0 },
		{ 1, 0, 0, 0 }, { 1, 0, 0, 0 },
		{ 0, 1, 0, 0 }, { 0, 1, 0, 0 }
	};
	
	alignas(16) const float voxelBitangents[][4] =
	{
		{ 0, 1, 0, 0 }, { 0, 1, 0, 0 },
		{ 0, 0, 1, 0 }, { 0, 0, 1, 0 },
		{ 1, 0, 0, 0 }, { 1, 0, 0, 0 }
	};
	
	//Velocity diffusion & collision
	std::for_each_n(std::execution::par_unseq, m_particlePos, m_numParticles, [&] (__m128& aPos)
	{
		uint32_t a = &aPos - m_particlePos;
		__m128 vel = m_particleVel[a];
		
		//Velocity diffusion
		for (uint16_t bI = 0; bI < m_numCloseParticles[a]; bI++)
		{
			uint16_t b = m_closeParticles[a][bI];
			__m128 velA = m_particleVel[a];
			__m128 velB = m_particleVel[b];
			
			__m128 sep     = _mm_sub_ps(m_particlePos[a], m_particlePos[b]);
			float dist     = std::sqrt(_mm_cvtss_f32(_mm_dp_ps(sep, sep, 0xFF)));
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
		vel = _mm_mul_ps(vel, _mm_load1_ps(&VEL_SCALE));
		m_particlePos2[a] = _mm_add_ps(m_particlePos[a], _mm_mul_ps(vel, dt4));
		
		alignas(16) static const float half[] = { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128& halfM = *reinterpret_cast<const __m128*>(half);
		
		//Collision detection. This works by looping over all voxels close to the particle (within CHECK_RAD), then
		// checking all faces that face from a solid voxel to an air voxel.
		for (int tr = 0; tr < 5; tr++)
		{
			__m128i centerVx = _mm_cvtps_epi32(_mm_floor_ps(m_particlePos2[a]));
			
			float minDist = 0;
			__m128 displaceNormal;
			
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
						if (IsVoxelAir(voxelCoordSolid))
							continue;
						
						for (size_t n = 0; n < std::size(voxelNormals); n++)
						{
							__m128i normal = *reinterpret_cast<const __m128i*>(voxelNormals[n]);
							__m128 normalF = _mm_cvtepi32_ps(normal);
							
							//This voxel cannot be solid
							if (!IsVoxelAir(_mm_add_epi32(voxelCoordSolid, normal)))
								continue;
							
							//A point on the plane going through this face
							__m128 planePoint = _mm_add_ps(
								_mm_add_ps(_mm_cvtepi32_ps(voxelCoordSolid), halfM),
								_mm_mul_ps(normalF, halfM)
							);
							
							float distC = _mm_cvtss_f32(_mm_dp_ps(_mm_sub_ps(m_particlePos2[a], planePoint), normalF, 0xFF));
							float distE = distC - m_particleRadius[a] * 0.5f;
							
							if (distE < minDist)
							{
								__m128 iPos = _mm_sub_ps(_mm_add_ps(m_particlePos2[a], _mm_mul_ps(_mm_load1_ps(&distC), normalF)), planePoint);
								float iDotT = _mm_cvtss_f32(_mm_dp_ps(iPos, *reinterpret_cast<const __m128*>(voxelTangents[n]), 0xFF));
								float iDotBT = _mm_cvtss_f32(_mm_dp_ps(iPos, *reinterpret_cast<const __m128*>(voxelBitangents[n]), 0xFF));
								float iDotN = _mm_cvtss_f32(_mm_dp_ps(iPos, normalF, 0xFF));
								
								//Checks that the intersection actually happened on the voxel's face
								if (std::abs(iDotT) <= 0.5f && std::abs(iDotBT) <= 0.5f && std::abs(iDotN) < 0.8f)
								{
									minDist = distE;
									displaceNormal = normalF;
								}
							}
						}
					}
				}
			}
			
			//Applies an impulse to the velocity
			if (minDist < 0.0f)
			{
				float impulseScale = _mm_cvtss_f32(_mm_dp_ps(vel, displaceNormal, 0xFF)) * minDist * IMPACT_COEFFICIENT;
				__m128 impulse = _mm_mul_ps(displaceNormal, _mm_load1_ps(&impulseScale));
				vel = _mm_add_ps(vel, impulse);
			}
			
			//Applies collision correction
			m_particlePos2[a] = _mm_sub_ps(m_particlePos2[a], _mm_mul_ps(displaceNormal, _mm_load1_ps(&minDist)));
		}
		
		m_particleVel2[a] = vel;
	});
	
	for (const std::shared_ptr<QueryAABB>& qaabb : m_queryAABBsBT)
	{
		int numIntersecting = 0;
		glm::vec3 waterVelocity;
		glm::vec3 buoyancy;
		
		for (uint32_t p = 0; p < m_numParticles; p++)
		{
			glm::vec3 pos = *reinterpret_cast<glm::vec3*>(&m_particlePos2[p]);
			eg::AABB particleAABB(pos - m_particleRadius[p], pos + m_particleRadius[p]);
			
			if (qaabb->m_aabbBT.Intersects(particleAABB))
			{
				numIntersecting++;
				waterVelocity += *reinterpret_cast<glm::vec3*>(&m_particleVel2[p]);
				buoyancy -= DirectionVector((Dir)m_particleGravity[p]);
			}
		}
		
		std::lock_guard<std::mutex> lock(qaabb->m_mutex);
		qaabb->m_results.numIntersecting = numIntersecting;
		qaabb->m_results.waterVelocity = waterVelocity;
		qaabb->m_results.buoyancy = buoyancy;
	}
}

static int* stepsPerSecondVar = eg::TweakVarInt("water_sps", 60, 1);

void WaterSimulator::ThreadTarget()
{
	using Clock = std::chrono::high_resolution_clock;
	
	bool firstStep = true;
	Clock::time_point lastStepEnd = Clock::now();
	while (true)
	{
		eg::AABB playerAABB;
		
		int changeGravityParticle = -1;
		Dir newGravity = Dir::PosX; 
		int stepsPerSecond;
		
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_run)
				break;
			if (!firstStep)
			{
				std::swap(m_particleVel2, m_particleVel);
				std::swap(m_particlePos2, m_particlePos);
			}
			else
			{
				firstStep = false;
			}
			stepsPerSecond = *stepsPerSecondVar;
			
			if (!m_changeGravityParticles.empty())
			{
				changeGravityParticle = m_changeGravityParticles.back().first;
				newGravity = m_changeGravityParticles.back().second;
				m_changeGravityParticles.pop_back();
			}
		}
		
		playerAABB.min.y = (playerAABB.min.y + playerAABB.max.y) / 2.0f;
		
		Step(1.0f / stepsPerSecond, playerAABB, changeGravityParticle, newGravity);
		
		m_lastUpdateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - lastStepEnd).count();
		
		std::this_thread::sleep_until(lastStepEnd + std::chrono::nanoseconds(1000000000LL / stepsPerSecond));
		lastStepEnd = Clock::now();
	}
}

eg::BufferRef WaterSimulator::GetPositionsBuffer() const
{
	return m_positionsBuffer;
}

void WaterSimulator::Update()
{
	if (m_numParticles == 0)
		return;
	
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		
		//Copies AABB data for the background thread
		m_queryAABBsBT.clear();
		for (int64_t i = (int64_t)m_queryAABBs.size() - 1 ; i >= 0; i--)
		{
			if (std::shared_ptr<QueryAABB> aabb = m_queryAABBs[i].lock())
			{
				aabb->m_aabbBT = aabb->m_aabbMT;
				m_queryAABBsBT.push_back(std::move(aabb));
			}
			else
			{
				m_queryAABBs[i].swap(m_queryAABBs.back());
				m_queryAABBs.pop_back();
			}
		}
		
		if (m_changeGravityParticleMT != -1)
		{
			m_changeGravityParticles.emplace_back(m_changeGravityParticleMT, m_newGravityMT);
			m_changeGravityParticleMT = -1;
		}
		
		std::memcpy(reinterpret_cast<void*>(m_particlePosMT), m_particlePos, m_numParticles * sizeof(float) * 4);
	}
	
	m_changeGravityParticleMT = -1;
	
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(sizeof(float) * 4 * m_numParticles);
	std::memcpy(uploadBuffer.Map(), m_particlePosMT, m_numParticles * sizeof(float) * 4);
	uploadBuffer.Flush();
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_positionsBuffer, uploadBuffer.offset, 0, uploadBuffer.range);
	m_positionsBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

std::pair<float, int> WaterSimulator::RayIntersect(const eg::Ray& ray) const
{
	float minDst = INFINITY;
	int particle = -1;
	for (uint32_t i = 0; i < m_numParticles; i++)
	{
		float dst;
		if (ray.Intersects(eg::Sphere(glm::vec3(m_particlePosMT[i]), 0.3f), dst))
		{
			if (dst > 0 && dst < minDst)
			{
				minDst = dst;
				particle = i;
			}
		}
	}
	return { minDst, particle };
}
